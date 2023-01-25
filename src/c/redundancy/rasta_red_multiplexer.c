#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <rasta/bsd_utils.h>
#include <rasta/event_system.h>
#include <rasta/rasta_red_multiplexer.h>
#include <rasta/rastahandle.h>
#include <rasta/rastaredundancy.h>
#include <rasta/rmemory.h>
#ifdef USE_UDP
#include <rasta/udp.h>
#endif
#ifdef USE_TCP
#include <rasta/tcp.h>
#endif
#include "../transport/rasta_transport_callbacks.h"
#include <rasta/rastautil.h>

/* --- Notifications --- */

/**
 * wrapper for parameter in the diagnose notification thread handler
 */
struct diagnose_notification_parameter_wrapper {
    /**
     * the used redundancy multiplexer
     */
    redundancy_mux *mux;

    /**
     * value of N_diagnose
     */
    int n_diagnose;

    /**
     * current value of N_missed
     */
    int n_missed;

    /**
     * current value of T_drift
     */
    unsigned long t_drift;

    /**
     * current value of T_drift2
     */
    unsigned long t_drift2;

    /**
     * associated id of the redundancy channel this notification origins from
     */
    unsigned long channel_id;
};

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

    logger_log(&w->mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy onNewConnection caller", "calling onNewConnection function");
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
    free(wrapper);

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy call onNewConnection", "called onNewConnection");
}

/**
 * the is the function that handles the call of the onDiagnosticsAvailable notification pointer.
 * this runs on the main thread
 * @param wrapper a wrapper that contains the mux and the diagnose data
 * @return unused
 */
void red_on_diagnostic_caller(struct diagnose_notification_parameter_wrapper *w) {
    logger_log(&w->mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy onDiagnostics caller", "calling onDiagnostics function");
    (*w->mux->notifications.on_diagnostics_available)(w->mux, w->n_diagnose, w->n_missed, w->t_drift, w->t_drift2, w->channel_id);

    w->mux->notifications_running = (unsigned short)(w->mux->notifications_running - 1);
}

/**
 * fires the onDiagnoseAvailable event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param mux the redundancy multiplexer that is used
 * @param n_diagnose the value of N_Diagnose
 * @param n_missed the current value of N_missed
 * @param t_drift the current value of T_drift
 * @param t_drift2 the current value of T_drift2
 * @param id the id of the redundancy channel
 */
void red_call_on_diagnostic(redundancy_mux *mux, int n_diagnose,
                            int n_missed, unsigned long t_drift, unsigned long t_drift2, unsigned long id) {
    if (mux->notifications.on_diagnostics_available == NULL) {
        // notification not set, do nothing
        return;
    }

    mux->notifications_running++;

    struct diagnose_notification_parameter_wrapper wrapper;
    wrapper.mux = mux;
    wrapper.n_diagnose = n_diagnose;
    wrapper.n_missed = n_missed;
    wrapper.t_drift = t_drift;
    wrapper.t_drift2 = t_drift2;
    wrapper.channel_id = id;

    red_on_diagnostic_caller(&wrapper);

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy call onDiagnostics", "called onDiagnostics");
}

/* --------------------- */

// HACK
int on_readable_event(void *handle);
int receive_packet(struct rasta_receive_handle *h, redundancy_mux *mux, struct receive_event_data *data) {
    unsigned char *buffer = rmalloc(sizeof(unsigned char) * MAX_DEFER_QUEUE_MSG_SIZE);
    struct sockaddr_in sender = {0};
    ssize_t len = abstract_receive_packet(mux, data, buffer, &sender, receive_callback);
    if (len == 0) {
        return 0;
    }
    if (len < 0) {
        return -1;
    }

    size_t len_remaining = len;
    size_t read_offset = 0;
    while (len_remaining > 0) {
        uint16_t currentPacketSize = leShortToHost(&buffer[read_offset]);
        struct RastaRedundancyPacket receivedPacket = handle_received_data(mux, buffer + read_offset, currentPacketSize);
        update_redundancy_channels(mux, data, receivedPacket, &sender, redundancy_channel_extension_callback);

        // Check that deferqueue can take new elements before calling red_f_receiveData
        rasta_redundancy_channel *channel = redundancy_mux_get_channel(mux, receivedPacket.data.sender_id);
        if (deferqueue_isfull(&channel->defer_q) || fifo_full(channel->fifo_recv)) {
            on_readable_event(h);
        }

        red_f_receiveData(channel, receivedPacket, data->channel_index);

        len_remaining -= currentPacketSize;
        read_offset += currentPacketSize;
    }
    rfree(buffer);
    return 0;
}

ssize_t abstract_receive_packet(redundancy_mux *mux, struct receive_event_data *data, unsigned char *buffer, struct sockaddr_in *sender, RastaReceiveFunction receive_callback) {
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "Receive called");

    ssize_t len = receive_callback(mux, data, buffer, sender);

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d received data on upd", data->channel_index);
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d received data len = %lu", data->channel_index, len);

    if (len < 0) {
        return -1;
    }

    if (!len) {
        return 0;
    }
    return len;
}

struct RastaRedundancyPacket handle_received_data(redundancy_mux *mux, unsigned char *buffer, ssize_t len) {
    struct RastaByteArray incomingData;
    incomingData.length = (unsigned int)len;
    incomingData.bytes = buffer;

    rasta_hashing_context_t test;
    struct crc_options options;

    test.hash_length = mux->sr_hashing_context.hash_length;
    test.algorithm = mux->sr_hashing_context.algorithm;
    allocateRastaByteArray(&test.key, mux->sr_hashing_context.key.length);
    rmemcpy(test.key.bytes, mux->sr_hashing_context.key.bytes, mux->sr_hashing_context.key.length);
    rmemcpy(&options, &mux->config.redundancy.crc_type, sizeof(mux->config.redundancy.crc_type));

    struct RastaRedundancyPacket receivedPacket = bytesToRastaRedundancyPacket(incomingData,
                                                                               options, &test);

    freeRastaByteArray(&test.key);
    return receivedPacket;
}

void update_redundancy_channels(redundancy_mux *mux, struct receive_event_data *data, struct RastaRedundancyPacket receivedPacket, struct sockaddr_in *sender, RedundancyChannelExtensionFunction extension_callback) {
    rasta_transport_channel connected_channel;
    connected_channel.ip_address = rmalloc(sizeof(char) * 15);
    sockaddr_to_host(*sender, connected_channel.ip_address);
    connected_channel.port = ntohs(sender->sin_port);

    // find associated redundancy channel
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        if (receivedPacket.data.sender_id == mux->connected_channels[i].associated_id) {
            // found redundancy channel with associated id
            // need to check if redundancy channel already knows ip & port of sender
            rasta_redundancy_channel channel = mux->connected_channels[i];
            if (channel.connected_channel_count < mux->port_count) {
                // not all remote transport channel endpoints discovered

                int is_channel_saved = 0;

                for (unsigned int j = 0; j < channel.connected_channel_count; ++j) {
                    if (channel.connected_channels[j].port == connected_channel.port &&
                        strcmp(connected_channel.ip_address, channel.connected_channels[j].ip_address) == 0) {
                        // channel is already saved
                        is_channel_saved = 1;
                    }
                }

                if (!is_channel_saved) {
                    // channel wasn't saved yet -> add to list
                    mux->connected_channels[i].connected_channels[channel.connected_channel_count].ip_address = connected_channel.ip_address;
                    mux->connected_channels[i].connected_channels[channel.connected_channel_count].port = connected_channel.port;

                    extension_callback(&mux->connected_channels[i].connected_channels[channel.connected_channel_count], data);

                    mux->connected_channels[i].connected_channel_count++;

                    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d discovered client transport channel %s:%d for connection to 0x%lX",
                               data->channel_index, connected_channel.ip_address, connected_channel.port, channel.associated_id);
                } else {
                    // temp channel no longer needed -> free memory
                    rfree(connected_channel.ip_address);
                    break;
                }
            }

            // call the receive function of the associated channel
            /*logger_log(&mux->logger, LOG_LEVEL_DEBUG, "MUX", "count=%d", mux->channel_count);
            for (int k = 0; k < mux->channel_count; ++k) {
                logger_log(&mux->logger, LOG_LEVEL_DEBUG, "MUX", "channel %d, id=%0x%lX", i, mux->connected_channels[i].associated_id);
            }*/
            return;
        }
    }

    // no associated channel found -> received message from new partner
    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux receive", "received pdu from unknown entity with id=0x%lX",
               (long unsigned int)receivedPacket.data.sender_id);
    rasta_redundancy_channel new_channel;
    red_f_init(mux->logger, mux->config, mux->port_count, receivedPacket.data.sender_id, &new_channel);
    new_channel.associated_id = receivedPacket.data.sender_id;
    // add transport channel to redundancy channel
    new_channel.connected_channels[0].ip_address = connected_channel.ip_address;
    new_channel.connected_channels[0].port = connected_channel.port;

    extension_callback(&new_channel.connected_channels[0], data);

    new_channel.connected_channel_count++;

    new_channel.is_open = 1;

    // reallocate memory for new client
    mux->connected_channels = rrealloc(mux->connected_channels, (mux->channel_count + 1) * sizeof(rasta_redundancy_channel));

    mux->connected_channels[mux->channel_count] = new_channel;
    mux->channel_count++;

    // fire new redundancy channel notification
    red_call_on_new_connection(mux, new_channel.associated_id);
}

#ifdef USE_TCP
fd_event *prepare_receive_event(struct receive_event_data *data) {
    fd_event *evt = rmalloc(sizeof(fd_event));
    struct receive_event_data *channel_event_data = rmalloc(sizeof(struct receive_event_data));

    *channel_event_data = *data;
    channel_event_data->event = evt;
    memset(evt, 0, sizeof(fd_event));
    evt->enabled = 1;
    evt->carry_data = channel_event_data;
    evt->callback = channel_receive_event;
    evt->fd = data->h->mux.transport_states[data->channel_index].file_descriptor;

    return evt;
}

#ifdef ENABLE_TLS
fd_event *prepare_tls_accept_event(fd_event *evt, struct rasta_connected_transport_channel_state *connection) {
    struct receive_event_data *channel_event_data = evt->carry_data;
    channel_event_data->ssl = connection->ssl;
    evt->fd = connection->file_descriptor;

    return evt;
}

int channel_accept_event(void *carry_data) {
    struct rasta_connected_transport_channel_state connection;
    struct receive_event_data *data = carry_data;

    connection.tls_config = &data->h->config.tls;

    logger_log(&data->h->mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux accept", "Socket ready to accept");
    tcp_accept_tls(&data->h->mux.transport_states[data->channel_index], &connection);

    fd_event *evt = prepare_receive_event(data);
    prepare_tls_accept_event(evt, &connection);

    add_fd_event(data->h->ev_sys, evt, EV_READABLE);
    return 0;
}
#else

int channel_accept_event(void *carry_data) {
    struct receive_event_data *data = carry_data;

    logger_log(&data->h->mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux accept", "Socket ready to accept");
    int socket = tcp_accept(&data->h->mux.transport_states[data->channel_index]);
    data->h->mux.transport_states[data->channel_index].file_descriptor = socket;
    fd_event *evt = prepare_receive_event(data);

    // TODO: Leaked event
    add_fd_event(data->h->ev_sys, evt, EV_READABLE);
    return 0;
}
#endif
#endif

void run_channel_diagnostics(struct rasta_handle *h, unsigned int channel_count, unsigned int channel_index) {
    for (unsigned int i = 0; i < channel_count; ++i) {
        rasta_redundancy_channel current = h->mux.connected_channels[i];
        int n_diagnose = h->mux.config.redundancy.n_diagnose;

        unsigned long channel_diag_start_time = current.connected_channels[channel_index].diagnostics_data.start_time;

        if (current_ts() - channel_diag_start_time >= (unsigned long)n_diagnose) {
            // increase n_missed by amount of messages that are not received

            // amount of missed packets
            int missed_count = current.diagnostics_packet_buffer.count -
                               current.connected_channels[channel_index].diagnostics_data.received_packets;

            // increase n_missed
            current.connected_channels[channel_index].diagnostics_data.n_missed += missed_count;

            // window finished, fire event
            // fire diagnostic notification
            red_call_on_diagnostic(&h->mux,
                                   h->mux.config.redundancy.n_diagnose,
                                   current.connected_channels[channel_index].diagnostics_data.n_missed,
                                   current.connected_channels[channel_index].diagnostics_data.t_drift,
                                   current.connected_channels[channel_index].diagnostics_data.t_drift2,
                                   current.associated_id);

            // reset values
            current.connected_channels[channel_index].diagnostics_data.n_missed = 0;
            current.connected_channels[channel_index].diagnostics_data.received_packets = 0;
            current.connected_channels[channel_index].diagnostics_data.t_drift = 0;
            current.connected_channels[channel_index].diagnostics_data.t_drift2 = 0;
            current.connected_channels[channel_index].diagnostics_data.start_time = current_ts();

            deferqueue_clear(&current.diagnostics_packet_buffer);
        }
    }
}

int channel_receive_event(void *carry_data) {
    struct receive_event_data *data = carry_data;
    struct rasta_handle *h = data->h;
    unsigned int mux_channel_count = h->mux.channel_count;

    run_channel_diagnostics(h, mux_channel_count, data->channel_index);

    // channel count might have changed due to removal of channels
    mux_channel_count = h->mux.channel_count;

    logger_log(&h->mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Channel %d calling receive",
               data->channel_index);

    int result = receive_packet(h->receive_handle, &h->mux, data);

    logger_log(&h->mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Channel %d receive done",
               data->channel_index);
    return !!result;
}

int channel_timeout_event(void *carry_data) {
    (void)carry_data;
    // TODO: I don't know what exactly this should handle.

    //  Right now, we are (mis-) using this only to timeout waiting for the handshake response.
    return 1;
}

/**
 * initializes the timeout event
 * @param event the event
 * @param t_data the carry data for the first event
 * @param mux the redundancy multiplexer that will contain the channels
 */
void init_channel_timeout_events(timed_event *event, struct timeout_event_data *t_data, struct redundancy_mux *mux, int channel_timeout_ms) {
    memset(event, 0, sizeof(timed_event));
    t_data->mux = mux;
    t_data->event = event;
    event->callback = channel_timeout_event;
    event->carry_data = t_data;
    event->interval = channel_timeout_ms * 1000000lu;
}

/* ----------------------------*/

void redundancy_mux_init_config(redundancy_mux *mux, struct logger_t logger, struct RastaConfigInfo config) {
    mux->logger = logger;
    // mux->listen_ports = listen_ports;
    mux->port_count = config.redundancy.connections.count;
    mux->config = config;
    mux->notifications_running = 0;

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", mux->port_count);

    // init and bind sockets
#ifdef USE_UDP
    mux->transport_states = rmalloc(mux->port_count * sizeof(struct rasta_transport_state));
    memset(mux->transport_states, 0, mux->port_count * sizeof(struct rasta_transport_state));
#endif
#ifdef USE_TCP
    mux->transport_states = rmalloc(mux->port_count * sizeof(struct rasta_transport_state));
    memset(mux->transport_states, 0, mux->port_count * sizeof(struct rasta_transport_state));
#endif

    // allocate memory for connected channels
    mux->connected_channels = rmalloc(sizeof(rasta_redundancy_channel));
    mux->channel_count = 0;

    // init notifications to NULL
    mux->notifications.on_diagnostics_available = NULL;
    mux->notifications.on_new_connection = NULL;

    // load ports that are specified in config
    if (mux->config.redundancy.connections.count > 0) {
        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "loading listen from config");

        mux->listen_ports = rmalloc(sizeof(uint16_t) * mux->config.redundancy.connections.count);
        for (unsigned int j = 0; j < mux->config.redundancy.connections.count; ++j) {

#ifdef USE_UDP
            // init socket
            udp_init(&mux->transport_states[j], &mux->config.tls);

            // bind socket to device and port
            udp_bind_device(&mux->transport_states[j],
                            (uint16_t)mux->config.redundancy.connections.data[j].port,
                            mux->config.redundancy.connections.data[j].ip);
#endif
#ifdef USE_TCP
            // // init socket
            // tcp_init(&mux->rasta_tcp_socket_states[j], &mux->config.tls);
            // tcp_bind_device(&mux->rasta_tcp_socket_states[j],
            //                 (uint16_t)mux->config.redundancy.connections.data[j].port,
            //                 mux->config.redundancy.connections.data[j].ip);
#endif
            mux->listen_ports[j] = (uint16_t)mux->config.redundancy.connections.data[j].port;
        }
    }

    // init hashing context
    mux->sr_hashing_context.hash_length = config.sending.md4_type;
    mux->sr_hashing_context.algorithm = config.sending.sr_hash_algorithm;

    if (mux->sr_hashing_context.algorithm == RASTA_ALGO_MD4) {
        // use MD4 IV as key
        rasta_md4_set_key(&mux->sr_hashing_context, config.sending.md4_a, config.sending.md4_b,
                          config.sending.md4_c, config.sending.md4_d);
    } else {
        // use the sr_hash_key
        allocateRastaByteArray(&mux->sr_hashing_context.key, sizeof(unsigned int));

        // convert unsigned in to byte array
        mux->sr_hashing_context.key.bytes[0] = (config.sending.sr_hash_key >> 24) & 0xFF;
        mux->sr_hashing_context.key.bytes[1] = (config.sending.sr_hash_key >> 16) & 0xFF;
        mux->sr_hashing_context.key.bytes[2] = (config.sending.sr_hash_key >> 8) & 0xFF;
        mux->sr_hashing_context.key.bytes[3] = (config.sending.sr_hash_key) & 0xFF;
    }

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "initialization done");
}

redundancy_mux redundancy_mux_init_(struct logger_t logger, uint16_t *listen_ports, unsigned int port_count, struct RastaConfigInfo config) {
    redundancy_mux mux;

    mux.logger = logger;
    mux.listen_ports = listen_ports;
    mux.port_count = port_count;
    mux.config = config;
    mux.notifications_running = 0;
    mux.notifications.on_diagnostics_available = NULL;
    mux.notifications.on_new_connection = NULL;
    logger_log(&mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", port_count);

    return mux;
}

#ifdef USE_TCP
redundancy_mux redundancy_mux_init(struct logger_t logger, uint16_t *listen_ports, unsigned int port_count, struct RastaConfigInfo config) {
    redundancy_mux mux = redundancy_mux_init_(logger, listen_ports, port_count, config);
    mux.transport_states = rmalloc(port_count * sizeof(int));

    // logger_log(&mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up tcp socket %d/%d", i + 1, port_count);
    // tcp_init(&mux.rasta_tcp_socket_states[i], &config.tls);
    // tcp_bind_device(&mux.rasta_tcp_socket_states[i], mux.listen_ports[i], mux.config.redundancy.connections.data[i].ip);
    return mux;
}
#else
#ifdef USE_UDP
redundancy_mux redundancy_mux_init(struct logger_t logger, uint16_t *listen_ports, unsigned int port_count, struct RastaConfigInfo config) {

    redundancy_mux mux = redundancy_mux_init_(logger, listen_ports, port_count, config);

    mux.transport_states = rmalloc(port_count * sizeof(struct rasta_transport_state));

    // set up udp sockets
    for (unsigned int i = 0; i < port_count; ++i) {

        // init and bind sockets + threads array
        logger_log(&mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up udp socket %d/%d", i + 1, port_count);
        udp_init(&mux.transport_states[i], &config.tls);
        udp_bind(&mux.transport_states[i], listen_ports[i]);
    }

    // allocate memory for connected channels
    mux.connected_channels = rmalloc(sizeof(rasta_redundancy_channel));
    mux.channel_count = 0;

    logger_log(&mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "initialization done");
    return mux;
}
#endif
#endif

void cleanup_rasta_states(redundancy_mux *mux, struct rasta_transport_state *rasta_transport_states, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) {
        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "closing socket %d/%d", i + 1, count);
        bsd_close(rasta_transport_states[i].file_descriptor);
    }

    // free arrays
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "freeing thread data");
    rfree(rasta_transport_states);

    mux->port_count = 0;
    freeRastaByteArray(&mux->sr_hashing_context.key);

    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux close", "redundancy multiplexer closed");
}

#ifdef USE_UDP
void redundancy_mux_close(redundancy_mux *mux) {

    // close the sockets of the transport channels
    cleanup_rasta_states(mux, mux->transport_states, mux->port_count);

    // close the redundancy channels
    for (unsigned int j = 0; j < mux->channel_count; ++j) {
        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "cleanup connected channel %d/%d", j + 1, mux->channel_count);
        red_f_cleanup(&mux->connected_channels[j]);
    }
    rfree(mux->connected_channels);
}
#endif

#ifdef USE_TCP
void redundancy_mux_close(redundancy_mux *mux) {
    // close the sockets of the transport channels
    cleanup_rasta_states(mux, mux->transport_states, mux->port_count);
}
#endif

rasta_redundancy_channel *redundancy_mux_get_channel(redundancy_mux *mux, unsigned long id) {
    // iterate over all known channels
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        // check if channel id == wanted id
        if (mux->connected_channels[i].associated_id == id) {
            return &mux->connected_channels[i];
        }
    }

    // wanted id is unknown, return NULL
    return NULL;
}

void redundancy_mux_set_config_id(redundancy_mux *mux, unsigned long id) {
    // only set if a channel is available
    if (mux->channel_count > 0) {
        mux->connected_channels[0].associated_id = id;
    }
}

void redundancy_mux_send(redundancy_mux *mux, struct RastaPacket data) {
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "sending a data packet to id 0x%lX",
               (long unsigned int)data.receiver_id);

    // get the channel to the remote entity by the data's received_id
    rasta_redundancy_channel *receiver = redundancy_mux_get_channel(mux, data.receiver_id);

    if (receiver == NULL) {
        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "redundancy channel with id=0x%lX unknown",
                   (long unsigned int)data.receiver_id);
        // not receiver found
        return;
    }
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "current seq_tx=%lu", receiver->seq_tx);

    // create packet to send and convert to byte array
    struct RastaRedundancyPacket packet = createRedundancyPacket(receiver->seq_tx, data, mux->config.redundancy.crc_type);
    struct RastaByteArray data_to_send = rastaRedundancyPacketToBytes(packet, &receiver->hashing_context);

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "redundancy packet created");

    // increase seq_tx
    receiver->seq_tx = receiver->seq_tx + 1;

    // send on every transport channels
    rasta_transport_channel channel;
    for (unsigned int i = 0; i < receiver->connected_channel_count; ++i) {
        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Sending on transport channel %d/%d",
                   i + 1, receiver->connected_channel_count);

        channel = receiver->connected_channels[i];

        send_callback(mux, data_to_send, channel, i);

        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Sent data over channel %s:%d",
                   channel.ip_address, channel.port);
    }

    freeRastaByteArray(&data_to_send);

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA Red send", "Data sent over all transport channels");
}

int redundancy_try_mux_retrieve(redundancy_mux *mux, unsigned long id, struct RastaPacket *out) {
    // get the channel by id
    rasta_redundancy_channel *target = redundancy_mux_get_channel(mux, id);

    if (target == NULL) {
        logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux retrieve", "entity with id 0x%lX not connected, passing", id);
        return 0;
    }

    struct RastaByteArray *element;

    if (fifo_get_size(target->fifo_recv) == 0) {
        return 0;
    }

    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux retrieve", "Found element in queue");

    element = fifo_pop(target->fifo_recv);

    struct RastaPacket packet = bytesToRastaPacket(*element, &target->hashing_context);

    freeRastaByteArray(element);
    rfree(element);

    *out = packet;
    return 1;
}

void redundancy_mux_wait_for_notifications(redundancy_mux *mux) {
    if (mux->notifications_running == 0) {
        logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "all notification threads finished");
        return;
    }
    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "waiting for %d notification thread(s) to finish", mux->notifications_running);
    while (mux->notifications_running > 0) {
        // busy wait
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "all notification threads finished");
}

void redundancy_mux_wait_for_entity(redundancy_mux *mux, unsigned long id) {
    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "waiting for entity with id=0x%lX", id);
    rasta_redundancy_channel *target = NULL;
    while (target == NULL) {
        target = redundancy_mux_get_channel(mux, id);
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "entity with id=0x%lX available", id);
}

int redundancy_mux_listen_channels(redundancy_mux *mux) {
    int result = 0;
    for (unsigned i = 0; i < mux->port_count; ++i) {
#ifdef USE_TCP
        if (mux->transport_states[i].file_descriptor == 0) {
            // init socket
            tcp_init(&mux->transport_states[i], &mux->config.tls);
            tcp_bind_device(&mux->transport_states[i],
                            (uint16_t)mux->config.redundancy.connections.data[i].port,
                            mux->config.redundancy.connections.data[i].ip);
            tcp_listen(&mux->transport_states[i]);
            result = 1;
        }
#endif
    }
    return result;
}

#ifdef USE_TCP
void redundancy_mux_connect(redundancy_mux *mux, unsigned int channel, char *host, uint16_t port) {
    // init socket
    tcp_init(&mux->transport_states[channel], &mux->config.tls);
    tcp_bind_device(&mux->transport_states[channel],
                    (uint16_t)mux->config.redundancy.connections.data[channel].port,
                    mux->config.redundancy.connections.data[channel].ip);
    tcp_connect(&mux->transport_states[channel], host, port);
    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux connect", "connection established, sleeping for 5 seconds");
    sleep(5);
}
#endif

void redundancy_mux_add_channel(redundancy_mux *mux, unsigned long id, struct RastaIPData *transport_channels) {
    rasta_redundancy_channel channel;
    red_f_init(mux->logger, mux->config, mux->port_count, id, &channel);

    // add transport channels
    for (unsigned int i = 0; i < mux->port_count; ++i) {
        rasta_red_add_transport_channel(&channel,
#ifdef USE_TCP
                                        mux->transport_states[i],
#endif
                                        transport_channels[i].ip,
                                        (uint16_t)transport_channels[i].port);
    }

    // reallocate memory for new client
    mux->connected_channels = rrealloc(mux->connected_channels, (mux->channel_count + 1) * sizeof(rasta_redundancy_channel));

    mux->connected_channels[mux->channel_count] = channel;
    mux->channel_count++;

    logger_log(&mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux add channel", "added new redundancy channel for ID=0x%lX", id);
}

void redundancy_mux_remove_channel(redundancy_mux *mux, unsigned long channel_id) {
    rasta_redundancy_channel *channel = redundancy_mux_get_channel(mux, channel_id);
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "removing channel with ID=0x%lX", channel_id);

    if (channel == NULL) {
        // no channel with given id
        return;
    }

    rasta_redundancy_channel *new_channels = rmalloc((mux->channel_count - 1) * sizeof(rasta_redundancy_channel));

    int newIndex = 0;
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        rasta_redundancy_channel c = mux->connected_channels[i];

        if (c.associated_id == channel_id) {
            logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "skipping channel with ID=0x%lX", c.associated_id);
#ifdef USE_TCP
            for (unsigned int i = 0; i < c.connected_channel_count; ++i) {
                rasta_transport_channel *channel = &c.connected_channels[i];
                bsd_close(channel->fd);
#ifdef ENABLE_TLS
                if (channel->ssl) {
                    wolfSSL_shutdown(channel->ssl);
                    wolfSSL_free(channel->ssl);
                }
#endif
            }
#endif
            // channel to remove, skip
            continue;
        }

        logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "copy channel with ID=0x%lX", c.associated_id);
        // otherwise copy to new channel array
        new_channels[newIndex] = c;
        newIndex++;
    }

    rfree(mux->connected_channels);
    mux->connected_channels = new_channels;
    mux->channel_count = newIndex;
    logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "%d channels left", mux->channel_count);
}

/**
 * gets the amount of messages in the receive queue of the connected channel with index @p redundancy_channel_index
 * @param mux the multiplexer that is used
 * @param redundancy_channel_index the index of the redundancy channel inside the mux connected_channels array
 * @return amount of messages in the queue
 */
unsigned int get_queue_msg_count(redundancy_mux *mux, unsigned int redundancy_channel_index) {
    if (redundancy_channel_index > mux->channel_count - 1) {
        // channel does not exist anymore
        return 0;
    }

    rasta_redundancy_channel channel = mux->connected_channels[redundancy_channel_index];

    if (channel.fifo_recv == NULL) {
        return 0;
    }
    unsigned int size = fifo_get_size(channel.fifo_recv);

    return size;
}

int redundancy_mux_try_retrieve_all(redundancy_mux *mux, struct RastaPacket *out) {
    for (unsigned int i = 0; i < mux->channel_count; i++) {
        unsigned int msg_count = get_queue_msg_count(mux, i);
        if (msg_count > 0) {
            logger_log(&mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux retrieve all", "channel with index %d has %u messages", i, msg_count);
            redundancy_try_mux_retrieve(mux, mux->connected_channels[i].associated_id, out);
            return 1;
        }
    }
    return 0;
}
