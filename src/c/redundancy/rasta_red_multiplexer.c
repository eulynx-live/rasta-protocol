#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rasta/bsd_utils.h>
#include <rasta/event_system.h>
#include <rasta/rasta_red_multiplexer.h>
#include <rasta/rastahandle.h>
#include <rasta/rastaredundancy.h>
#include <rasta/rmemory.h>
#include <rasta/rastautil.h>
#include "../retransmission/safety_retransmission.h"
#include "../transport/transport.h"
#include "../transport/events.h"

/* --- Notifications --- */

/**
 * wrapper for parameter in the onNewNotification notification thread handler
 */
struct new_connection_notification_parameter_wrapper {
    /**
     * the used redundancy multiplexer
     */
    redundancy_mux *mux;

    /**
     * the id of the new redundancy channel
     */
    unsigned long id;
};

/**
 * the is the function that handles the call of the onDiagnosticsAvailable notification pointer.
 * this runs on the main thread
 * @param connection the connection that will be used
 * @return unused
 */
void red_on_new_connection_caller(struct new_connection_notification_parameter_wrapper *w) {

    logger_log(w->mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy onNewConnection caller", "calling onNewConnection function");
    (*w->mux->notifications.on_new_connection)(w->mux, w->id);

    w->mux->notifications_running = (unsigned short)(w->mux->notifications_running - 1);
}

/**
 * fires the onDiagnoseAvailable event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param mux the redundancy multiplexer that is used
 * @param id the id of the new redundacy channel
 */
void red_call_on_new_connection(redundancy_mux *mux, unsigned long id) {
    if (mux->notifications.on_new_connection == NULL) {
        // notification not set, do nothing
        return;
    }

    mux->notifications_running++;

    struct new_connection_notification_parameter_wrapper *wrapper =
        rmalloc(sizeof(struct new_connection_notification_parameter_wrapper));
    wrapper->mux = mux;
    wrapper->id = id;

    red_on_new_connection_caller(wrapper);
    rfree(wrapper);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy call onNewConnection", "called onNewConnection");
}

/* --------------------- */

int receive_packet(redundancy_mux *mux, rasta_transport_channel *transport_channel, unsigned char *buffer, size_t len) {
    int result = 0;
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d received data len = %lu", transport_channel->id, len);

    size_t len_remaining = len;
    size_t read_offset = 0;
    while (len_remaining > 0) {
        uint16_t currentPacketSize = leShortToHost(&buffer[read_offset]);
        struct RastaRedundancyPacket receivedPacket;
        handle_received_data(mux, buffer + read_offset, currentPacketSize, &receivedPacket);
        // Check that deferqueue can take new elements before calling red_f_receiveData
        rasta_redundancy_channel *channel = redundancy_mux_get_channel(mux, receivedPacket.data.sender_id);
        if (deferqueue_isfull(&channel->defer_q)) {
            // Discard incoming packet
            logger_log(channel->logger, LOG_LEVEL_INFO, "RaSTA Red receive", "discarding packet because defer queue is full");
        } else {
            // If this turned out to be a new connection or new application data, break from processing
            result |= red_f_receiveData(channel, receivedPacket, transport_channel->id);
        }

        len_remaining -= currentPacketSize;
        read_offset += currentPacketSize;
    }

    return result;
}

int handle_closed_transport(rasta_connection *connection, rasta_redundancy_channel *channel) {
    for (unsigned i = 0; i < channel->transport_channel_count; i++) {
        if (channel->transport_channels[i].connected) {
            // Another channel is still connected, continue the event loop
            return 0;
        }
    }

    sr_closed_connection(connection, channel->associated_id);
    return 1;
}

void handle_received_data(redundancy_mux *mux, unsigned char *buffer, ssize_t len, struct RastaRedundancyPacket *receivedPacket) {
    struct RastaByteArray incomingData;
    incomingData.length = (unsigned int)len;
    incomingData.bytes = buffer;

    rasta_hashing_context_t test;
    struct crc_options options;

    test.hash_length = mux->sr_hashing_context.hash_length;
    test.algorithm = mux->sr_hashing_context.algorithm;
    allocateRastaByteArray(&test.key, mux->sr_hashing_context.key.length);
    rmemcpy(test.key.bytes, mux->sr_hashing_context.key.bytes, mux->sr_hashing_context.key.length);
    rmemcpy(&options, &mux->config->redundancy.crc_type, sizeof(mux->config->redundancy.crc_type));

    bytesToRastaRedundancyPacket(incomingData, options, &test, receivedPacket);

    freeRastaByteArray(&test.key);
}

int channel_timeout_event(void *carry_data) {
    UNUSED(carry_data);
    // Escape the event loop
    return 1;
}

/**
 * initializes the timeout event
 * @param event the event
 * @param t_data the carry data for the first event
 * @param mux the redundancy multiplexer that will contain the channels
 */
void init_handshake_timeout_event(timed_event *event, int channel_timeout_ms) {
    memset(event, 0, sizeof(timed_event));
    event->callback = channel_timeout_event;
    event->interval = channel_timeout_ms * 1000000lu;
}

/* ----------------------------*/

void redundancy_mux_init_config(redundancy_mux *mux, struct logger_t *logger, rasta_config_info *config) {
    mux->logger = logger;
    mux->port_count = config->redundancy.connections.count;
    mux->listen_ports = rmalloc(sizeof(uint16_t) * mux->port_count);
    mux->config = config;
    mux->notifications_running = 0;

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", mux->port_count);

    // init notifications to NULL
    mux->notifications.on_diagnostics_available = NULL;
    mux->notifications.on_new_connection = NULL;

    // init hashing context
    // TODO: Why is this code duplicated in the safety/retransmission layer?
    mux->sr_hashing_context.hash_length = config->sending.md4_type;
    mux->sr_hashing_context.algorithm = config->sending.sr_hash_algorithm;

    if (mux->sr_hashing_context.algorithm == RASTA_ALGO_MD4) {
        // use MD4 IV as key
        rasta_md4_set_key(&mux->sr_hashing_context, config->sending.md4_a, config->sending.md4_b,
                          config->sending.md4_c, config->sending.md4_d);
    } else {
        // use the sr_hash_key
        allocateRastaByteArray(&mux->sr_hashing_context.key, sizeof(unsigned int));

        // convert unsigned in to byte array
        mux->sr_hashing_context.key.bytes[0] = (config->sending.sr_hash_key >> 24) & 0xFF;
        mux->sr_hashing_context.key.bytes[1] = (config->sending.sr_hash_key >> 16) & 0xFF;
        mux->sr_hashing_context.key.bytes[2] = (config->sending.sr_hash_key >> 8) & 0xFF;
        mux->sr_hashing_context.key.bytes[3] = (config->sending.sr_hash_key) & 0xFF;
    }

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "initialization done");
}

redundancy_mux redundancy_mux_init_(struct logger_t *logger, uint16_t *listen_ports, unsigned int port_count, rasta_config_info *config) {
    redundancy_mux mux;

    mux.logger = logger;
    mux.listen_ports = listen_ports;
    mux.port_count = port_count;
    mux.config = config;
    mux.notifications_running = 0;
    mux.notifications.on_diagnostics_available = NULL;
    mux.notifications.on_new_connection = NULL;
    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", port_count);

    return mux;
}

redundancy_mux redundancy_mux_init(struct logger_t *logger, uint16_t *listen_ports, unsigned int port_count, rasta_config_info *config) {
    redundancy_mux mux = redundancy_mux_init_(logger, listen_ports, port_count, config);
    mux.transport_sockets = rmalloc(port_count * sizeof(int));

    // logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up tcp socket %d/%d", i + 1, port_count);
    // tcp_init(&mux.rasta_tcp_socket_states[i], &config.tls);
    // tcp_bind_device(&mux.rasta_tcp_socket_states[i], mux.listen_ports[i], mux.config.redundancy.connections.data[i].ip);
    return mux;
}

void redundancy_mux_allocate_channels(struct rasta_handle *h, redundancy_mux *mux, rasta_connection_config *connections, size_t connections_length) {
    mux->redundancy_channels = rmalloc(sizeof(rasta_redundancy_channel) * connections_length);
    mux->redundancy_channels_count = connections_length;

    // load ports that are specified in config
    if (mux->config->redundancy.connections.count > 0) {
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "loading listen from config");

        // init sockets
        mux->transport_sockets = rmalloc(mux->port_count * sizeof(rasta_transport_socket));
        memset(mux->transport_sockets, 0, mux->port_count * sizeof(rasta_transport_socket));
        for (unsigned i = 0; i < mux->port_count; i++) {
            transport_create_socket(h, &mux->transport_sockets[i], i, &mux->config->tls);
        }
    }

    for (unsigned i = 0; i < connections_length; i++) {
        assert(connections[i].transport_sockets_count == mux->port_count);
        red_f_init(h, mux->logger, connections[i].config, connections[i].transport_sockets, connections[i].transport_sockets_count,
            connections[i].rasta_id, &mux->redundancy_channels[i]);
        mux->redundancy_channels[i].mux = mux;
    }
}

bool redundancy_mux_bind(struct rasta_handle *h) {
    bool success = false;
    for (unsigned i = 0; i < h->mux.port_count; ++i) {
        const rasta_ip_data *ip_data = &h->mux.config->redundancy.connections.data[i];
        success |= transport_bind(h, &h->mux.transport_sockets[i], ip_data->ip, (uint16_t)ip_data->port);
    }
    return success;
}

void redundancy_mux_close(redundancy_mux *mux) {
    // TODO: red_f_cleanup should be called when closing a rasta_connection

    // Close listening ports (if not already closed for the case that we are a client)
    for (unsigned int i = 0; i < mux->port_count; ++i) {
        if (mux->transport_sockets[i].file_descriptor != -1) {
            logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "closing socket %d/%d", i + 1, mux->port_count);
            bsd_close(mux->transport_sockets[i].file_descriptor);
        }
    }

    mux->port_count = 0;
    rfree(mux->transport_sockets);
    for (unsigned int i = 0; i < mux->redundancy_channels_count; i++) {
        rfree(mux->redundancy_channels[i].transport_channels);
    }
    rfree(mux->redundancy_channels);

    freeRastaByteArray(&mux->sr_hashing_context.key);
}

rasta_redundancy_channel *redundancy_mux_get_channel(redundancy_mux *mux, unsigned long id) {
    // iterate over all known channels
    for (unsigned int i = 0; i < mux->redundancy_channels_count; ++i) {
        // check if channel id == wanted id
        if (mux->redundancy_channels[i].associated_id == id) {
            return &mux->redundancy_channels[i];
        }
    }

    // wanted id is unknown, return NULL
    return NULL;
}

void redundancy_mux_send(rasta_redundancy_channel *receiver, struct RastaPacket *data, rasta_role role) {
    redundancy_mux *mux = receiver->mux;
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "sending a data packet to id 0x%lX",
               (long unsigned int)data->receiver_id);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "current seq_tx=%lu", receiver->seq_tx);

    // create packet to send and convert to byte array
    struct RastaRedundancyPacket packet;
    createRedundancyPacket(receiver->seq_tx, data, mux->config->redundancy.crc_type, &packet);
    struct RastaByteArray data_to_send = rastaRedundancyPacketToBytes(&packet, &receiver->hashing_context);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "redundancy packet created");

    // increase seq_tx
    receiver->seq_tx = receiver->seq_tx + 1;

    // send on every transport channel
    for (unsigned int i = 0; i < receiver->transport_channel_count; ++i) {
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Sending on transport channel %d/%d",
                   i + 1, receiver->transport_channel_count);

        rasta_transport_channel *channel = &receiver->transport_channels[i];

        if (!channel->connected) {
            // Attempt to connect (maybe previous attempts were unsuccessful)
            // only a RaSTA client can initiate reconnect
            if (role == RASTA_ROLE_CLIENT) {
                logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Channel %d/%d is not connected, re-trying %s:%d",
                    i + 1, receiver->transport_channel_count, channel->remote_ip_address, channel->remote_port);
                rasta_transport_socket *socket = &mux->transport_sockets[channel->id];
                if (transport_redial(channel, socket) != 0) {
                    continue;
                }
                logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Reconnected channel %d/%d",
                i + 1, receiver->transport_channel_count);
            } else {
                logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Skipping unconnected channel %d/%d",
                i + 1, receiver->transport_channel_count);
                continue;
            }
        }

        channel->send_callback(mux, data_to_send, channel, i);

        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Sent data over channel %s:%d",
                channel->remote_ip_address, channel->remote_port);
    }

    freeRastaByteArray(&data_to_send);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA Red send", "Data sent over all transport channels");
}

// TODO: Remove this and next method because it scares me. Only used from tests though.

void redundancy_mux_wait_for_notifications(redundancy_mux *mux) {
    if (mux->notifications_running == 0) {
        logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "all notification threads finished");
        return;
    }
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "waiting for %d notification thread(s) to finish", mux->notifications_running);
    while (mux->notifications_running > 0) {
        // busy wait
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "all notification threads finished");
}

void redundancy_mux_wait_for_entity(redundancy_mux *mux, unsigned long id) {
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "waiting for entity with id=0x%lX", id);
    rasta_redundancy_channel *target = NULL;
    while (target == NULL) {
        target = redundancy_mux_get_channel(mux, id);
        // to avoid too much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "entity with id=0x%lX available", id);
}

void redundancy_mux_listen_channels(struct rasta_handle *h, redundancy_mux *mux) {
    for (unsigned i = 0; i < mux->port_count; ++i) {
        transport_listen(h, &mux->transport_sockets[i]);
    }
}

int rasta_red_connect_transport_channel(rasta_connection *h, rasta_redundancy_channel *channel, rasta_transport_socket *transport_socket) {
    rasta_transport_channel *transport_connection = &channel->transport_channels[transport_socket->id];
    transport_connect(transport_socket, transport_connection, h->config->tls);
    return transport_connection->connected;
}

int redundancy_mux_connect_channel(rasta_connection *connection, redundancy_mux *mux, rasta_redundancy_channel *channel) {
    // add transport channels
    int success = 0;
    for (unsigned int i = 0; i < channel->transport_channel_count; i++) {
        // Provided transport channels have to match with local ports configured
        success |= rasta_red_connect_transport_channel(connection, channel, &mux->transport_sockets[i]);
#ifdef SLEEP_ON_CONNECT
        if (success) {
            logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux connect", "connection established, sleeping for 5 seconds");
            sleep(5);
        }
#endif
    }

    if (!success) {
        red_f_cleanup(channel);
        return 1;
    }

    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux add channel", "added new redundancy channel for ID=0x%lX", channel->associated_id);

    return 0;
}

void redundancy_mux_close_channel(rasta_redundancy_channel *c) {
    for (unsigned int i = 0; i < c->transport_channel_count; ++i) {
        rasta_transport_channel *channel = &c->transport_channels[i];
        logger_log(c->mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "closing transport channel %u/%u", i+1, c->transport_channel_count);
        int channel_fd = channel->file_descriptor;
        transport_close(channel);
        // if we are a TCP/TLS client (and transport_close actually closes the channel), the socket fd also becomes invalid
        if(!channel->connected && channel_fd == c->mux->transport_sockets[channel->id].file_descriptor) {
            c->mux->transport_sockets[channel->id].file_descriptor = -1;
        }
    }
}
