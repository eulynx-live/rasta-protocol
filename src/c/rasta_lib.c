#include <rasta/rasta_lib.h>

#include <memory.h>
#include <stdbool.h>
#include <rasta/rasta.h>
#include <rasta/rmemory.h>
#include "transport/transport.h"
#include "transport/events.h"
#include "retransmission/protocol.h"
#include "retransmission/safety_retransmission.h"

void init_connection_timeout_event(timed_event *ev, struct timed_event_data *carry_data,
                                   struct rasta_connection *connection) {
    memset(ev, 0, sizeof(timed_event));
    ev->callback = event_connection_expired;
    ev->carry_data = carry_data;
    ev->interval = connection->config->sending.t_max * 1000000lu;
    carry_data->handle = &connection->heartbeat_handle;
    carry_data->connection = connection;
}

void init_send_heartbeat_event(timed_event *ev, struct timed_event_data *carry_data,
                               struct rasta_connection *connection) {
    memset(ev, 0, sizeof(timed_event));
    ev->callback = heartbeat_send_event;
    ev->carry_data = carry_data;
    ev->interval = connection->config->sending.t_h * 1000000lu;
    carry_data->handle = &connection->heartbeat_handle;
    carry_data->connection = connection;
}

void init_send_key_exchange_event(timed_event *ev, struct timed_event_data *carry_data,
                                  struct rasta_connection *connection) {
    ev->callback = send_timed_key_exchange;
    ev->carry_data = carry_data;
    ev->interval = connection->config->kex.rekeying_interval_ms * NS_PER_MS;
    // add some headroom for computation and communication
    ev->interval /= 2;
    carry_data->handle = &connection->receive_handle;
    carry_data->connection = connection;
}

void init_connection_events(struct rasta_handle *h, struct rasta_connection *connection) {
    init_handshake_timeout_event(&connection->handshake_timeout_event, h->config->sending.t_max);
    init_connection_timeout_event(&connection->timeout_event, &connection->timeout_carry_data, connection);
    init_send_heartbeat_event(&connection->send_heartbeat_event, &connection->timeout_carry_data, connection);

    add_timed_event(h->ev_sys, &connection->handshake_timeout_event);
    add_timed_event(h->ev_sys, &connection->timeout_event);
    add_timed_event(h->ev_sys, &connection->send_heartbeat_event);
#ifdef ENABLE_OPAQUE
    if (connection->role == RASTA_ROLE_CLIENT && h->config->kex.rekeying_interval_ms) {
        init_send_key_exchange_event(&connection->rekeying_event, &connection->rekeying_carry_data, connection);
        add_timed_event(h->ev_sys, &connection->rekeying_event);
    }
#endif
}

// This is the time that packets are deferred for creating multi-packet messages
// See section 5.5.10
#define IO_INTERVAL 10000

void rasta_lib_init_configuration(rasta_lib_configuration_t user_configuration, rasta_config_info *config, struct logger_t *logger, rasta_connection_config *connections, size_t connections_length) {
    memset(user_configuration, 0, sizeof(rasta_lib_configuration_t));
    rasta_socket(&user_configuration->h, config, logger);
    memset(&user_configuration->rasta_lib_event_system, 0, sizeof(user_configuration->rasta_lib_event_system));
    memset(&user_configuration->callback, 0, sizeof(user_configuration->callback));
    user_configuration->h.user_handles = &user_configuration->callback;

    struct rasta_handle *h = &user_configuration->h;
    event_system *event_system = &user_configuration->rasta_lib_event_system;
    h->ev_sys = event_system;

    // init the redundancy layer
    redundancy_mux_init_config(&h->mux, h->logger, config);

    // This is the place where we malloc
    redundancy_mux_allocate_channels(h, &h->mux, connections, connections_length);

    h->rasta_connections = rmalloc(sizeof(rasta_connection) * connections_length);
    memset(h->rasta_connections, 0, sizeof(rasta_connection) * connections_length);
    for (unsigned i = 0; i < connections_length; i++) {
        rasta_connection *connection = &h->rasta_connections[i];
        sr_reset_connection(connection);

        connection->config = connections[i].config;
        connection->logger = h->logger;
        connection->remote_id = connections[i].rasta_id;
        connection->my_id = (uint32_t)connection->config->general.rasta_id;
        connection->network_id = (uint32_t)connection->config->general.rasta_network;

        // This is a little hacky
        connection->redundancy_channel = &h->mux.redundancy_channels[i];
        for (unsigned j = 0; j < connection->redundancy_channel->transport_channel_count; j++) {
            connection->redundancy_channel->transport_channels[j].receive_event_data.connection = connection;
        }

        // batch outgoing packets
        memset(&connection->send_handle.send_event, 0, sizeof(timed_event));
        connection->send_handle.send_event.callback = data_send_event;
        connection->send_handle.send_event.interval = IO_INTERVAL * 1000lu;
        connection->send_handle.send_event.carry_data = &connection->send_handle;
        connection->send_handle.connection = connection;

        add_timed_event(h->ev_sys, &connection->send_handle.send_event);

        connection->send_handle.config = &config->sending;
        connection->send_handle.info = &config->general;
        connection->send_handle.logger = logger;
        connection->send_handle.mux = connection->redundancy_channel->mux;
        connection->send_handle.hashing_context = &h->mux.sr_hashing_context;

        // heartbeat
        connection->heartbeat_handle.logger = logger;
        connection->heartbeat_handle.mux = connection->redundancy_channel->mux;
        connection->heartbeat_handle.hashing_context = &connection->redundancy_channel->mux->sr_hashing_context;

        // receive
        connection->receive_handle.config = &config->sending;
        connection->receive_handle.info = &config->general;
        connection->receive_handle.handle = h;
        connection->receive_handle.logger = logger;
        connection->receive_handle.hashing_context = &h->mux.sr_hashing_context;

        // init retransmission fifo
        connection->fifo_retransmission = fifo_init(MAX_QUEUE_SIZE);

        // create send queue
        connection->fifo_send = fifo_init(2 * connection->config->sending.max_packet);

        init_connection_events(h, connection);
    }
    
    h->rasta_connections_length = connections_length;
}
