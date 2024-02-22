#include <memory.h>
#include <stdbool.h>

#include <rasta/rasta.h>

#include "rasta_connection.h"
#include "retransmission/safety_retransmission.h"
#include "transport/events.h"
#include "transport/transport.h"
#include "util/rastautil.h"
#include "util/rmemory.h"

// This is the time that packets are deferred for creating multi-packet messages (in ms)
// See section 5.5.10
#define IO_INTERVAL 10

void init_connection_timeout_event(timed_event *ev, struct timed_event_data *carry_data,
                                   struct rasta_connection *connection) {
    memset(ev, 0, sizeof(timed_event));
    ev->callback = event_connection_expired;
    ev->carry_data = carry_data;
    ev->interval = connection->config->sending.t_max * NS_PER_MS;
    carry_data->handle = &connection->heartbeat_handle;
    carry_data->connection = connection;
}

void init_send_heartbeat_event(timed_event *ev, struct timed_event_data *carry_data,
                               struct rasta_connection *connection) {
    memset(ev, 0, sizeof(timed_event));
    ev->callback = heartbeat_send_event;
    ev->carry_data = carry_data;
    ev->interval = connection->config->sending.t_h * NS_PER_MS;
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

void rasta_socket(rasta *user_configuration, rasta_config_info *config, struct logger_t *logger) {
    struct rasta_handle *handle = &user_configuration->h;
    rasta_handle_init(handle, config, logger);

    //  register redundancy layer diagnose notification handler
    handle->mux.notifications.on_diagnostics_available = handle->notifications.on_redundancy_diagnostic_notification;
}

rasta *rasta_lib_init_configuration(rasta_config_info *config, log_level log_level, logger_type logger_type) {
    rasta *user_configuration = rmalloc(sizeof(rasta));
    memset(user_configuration, 0, sizeof(rasta));
    logger_init(&user_configuration->logger, log_level, logger_type);
    rasta_socket(user_configuration, config, &user_configuration->logger);
    memset(&user_configuration->rasta_lib_event_system, 0, sizeof(user_configuration->rasta_lib_event_system));

    struct rasta_handle *h = &user_configuration->h;
    event_system *event_system = &user_configuration->rasta_lib_event_system;
    h->ev_sys = event_system;

    // init the redundancy layer
    // This is the place where we malloc
    redundancy_mux_alloc(h, &h->mux, h->logger, config);

    h->rasta_connection = rmalloc(sizeof(rasta_connection));
    memset(h->rasta_connection, 0, sizeof(rasta_connection));

    rasta_connection *connection = h->rasta_connection;
    sr_reset_connection(connection);

    connection->config = config;
    connection->logger = h->logger;
    connection->remote_id = config->general.rasta_id_remote;
    connection->my_id = (uint32_t)connection->config->general.rasta_id;
    connection->network_id = (uint32_t)connection->config->general.rasta_network;

    // This is a little hacky
    connection->redundancy_channel = h->mux.redundancy_channel;
    for (unsigned j = 0; j < connection->redundancy_channel->transport_channel_count; j++) {
        connection->redundancy_channel->transport_channels[j].receive_event_data.connection = connection;
    }

    // batch outgoing packets
    memset(&connection->send_handle.send_event, 0, sizeof(timed_event));
    connection->send_handle.send_event.callback = data_send_event;
    connection->send_handle.send_event.interval = IO_INTERVAL * NS_PER_MS;
    connection->send_handle.send_event.carry_data = &connection->send_handle;
    connection->send_handle.connection = connection;

    add_timed_event(h->ev_sys, &connection->send_handle.send_event);

    connection->send_handle.config = &config->sending;
    connection->send_handle.info = &config->general;
    connection->send_handle.logger = &user_configuration->logger;
    connection->send_handle.mux = connection->redundancy_channel->mux;
    connection->send_handle.hashing_context = &h->mux.sr_hashing_context;

    // heartbeat
    connection->heartbeat_handle.logger = &user_configuration->logger;
    connection->heartbeat_handle.mux = connection->redundancy_channel->mux;
    connection->heartbeat_handle.hashing_context = &connection->redundancy_channel->mux->sr_hashing_context;

    // receive
    connection->receive_handle.config = &config->sending;
    connection->receive_handle.info = &config->general;
    connection->receive_handle.handle = h;
    connection->receive_handle.logger = &user_configuration->logger;
    connection->receive_handle.hashing_context = &h->mux.sr_hashing_context;

    // init retransmission fifo
    connection->fifo_retransmission = fifo_init(connection->config->retransmission.max_retransmission_queue_size);

    // create send queue
    connection->fifo_send = fifo_init(2 * connection->config->sending.max_packet);

    // init receive queue
    connection->fifo_receive = fifo_init(connection->config->receive.max_recvqueue_size);

    init_connection_events(h, connection);

    return user_configuration;
}
