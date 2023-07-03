#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <rasta/config.h>
#include <rasta/event_system.h>
#include <rasta/rasta.h>
#include <rasta/rasta_lib.h>
#include <rasta/rastahandle.h>
#include <rasta/rastaredundancy.h>
#include <rasta/rmemory.h>

#include "experimental/handlers.h"
#include "retransmission/handlers.h"
#include "retransmission/messages.h"
#include "retransmission/protocol.h"
#include "retransmission/safety_retransmission.h"

/**
 * send a Key Exchange Request to the specified host
 * @param connection the connection which should be used
 * @param host the host where the HB will be sent to
 * @param port the port where the HB will be sent to
 */
void init_send_key_exchange_event(timed_event *ev, struct timed_event_data *carry_data,
                                  struct rasta_connection *connection);
void send_KexRequest(struct rasta_connection *connection) {
#ifdef ENABLE_OPAQUE
    struct RastaPacket hb = createKexRequest(connection->remote_id, connection->my_id, connection->sn_t,
                                             connection->cs_t, cur_timestamp(), connection->ts_r,
                                             &connection->redundancy_channel->mux->sr_hashing_context, connection->config->kex.psk, &connection->kex_state, connection->logger);

    if (!connection->kex_state.last_key_exchanged_millis && connection->config->kex.rekeying_interval_ms) {
        // first key exchanged - need to enable periodic rekeying
        init_send_key_exchange_event(&connection->rekeying_event, &connection->rekeying_carry_data, connection);
        // TODO: Register event (somewhere else)
        enable_timed_event(&connection->rekeying_event);
    } else {
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA KEX", "Rekeying at %" PRIu64, get_current_time_ms());
    }

    redundancy_mux_send(connection->redundancy_channel, &hb, connection->role);

    connection->sn_t = connection->sn_t + 1;

    connection->kex_state.last_key_exchanged_millis = get_current_time_ms();
    connection->current_state = RASTA_CONNECTION_KEX_RESP;
#else
    // should never be called
    (void)connection;
    abort();
#endif
}

int send_timed_key_exchange(void *arg) {
#ifdef ENABLE_OPAQUE
    struct timed_event_data *event_data = (struct timed_event_data *)arg;
    // rasta_receive_handle *handle = (rasta_receive_handle *)event_data->handle;
    send_KexRequest(event_data->connection);
    // call periodically
    reschedule_event(&event_data->connection->rekeying_event);
#else
    // should never be called
    (void)arg;
#endif
    return 0;
}

int rasta_receive(struct rasta_connection *con, struct RastaPacket *receivedPacket) {
    switch (receivedPacket->type) {
        case RASTA_TYPE_RETRDATA:
            return handle_retrdata(con, receivedPacket);
        case RASTA_TYPE_DATA:
            return handle_data(con, receivedPacket);
        case RASTA_TYPE_RETRREQ:
            return handle_retrreq(con, receivedPacket);
        case RASTA_TYPE_RETRRESP:
            return handle_retrresp(con, receivedPacket);
        case RASTA_TYPE_DISCREQ:
            return handle_discreq(con, receivedPacket);
        case RASTA_TYPE_HB:
            return handle_hb(con, receivedPacket);
#ifdef ENABLE_OPAQUE
        case RASTA_TYPE_KEX_REQUEST:
            return handle_kex_request(con, receivedPacket);
        case RASTA_TYPE_KEX_RESPONSE:
            return handle_kex_response(con, receivedPacket);
        case RASTA_TYPE_KEX_AUTHENTICATION:
            return handle_kex_auth(con, receivedPacket);
#endif
        default:
            logger_log(con->logger, LOG_LEVEL_ERROR, "RaSTA RECEIVE", "Received unexpected packet type %d", receivedPacket->type);
            // increase type error counter
            con->errors.type++;
            break;
    }
    return 0;
}

int event_connection_expired(void *carry_data) {
    struct timed_event_data *data = carry_data;
    struct rasta_heartbeat_handle *h = (struct rasta_heartbeat_handle *)data->handle;
    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA HEARTBEAT", "T_i timer expired");

    struct rasta_connection *connection = data->connection;
    // so check if connection is valid

    if (connection == NULL) {
        logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA HEARTBEAT", "connection is unknown");
        return 0;
    }

    if (connection->hb_locked) {
        logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA HEARTBEAT", "connection is hb_locked");
        return 0;
    }

    // connection is valid, check current state
    if (connection->current_state == RASTA_CONNECTION_UP || connection->current_state == RASTA_CONNECTION_RETRREQ || connection->current_state == RASTA_CONNECTION_RETRRUN) {

        // fire heartbeat timeout event
        fire_on_heartbeat_timeout(sr_create_notification_result(NULL, connection));

        // T_i expired -> close connection
        sr_close_connection(connection, RASTA_DISC_REASON_TIMEOUT, 0);
        logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA HEARTBEAT", "T_i timer expired - \033[91mdisconnected\033[0m");
    }

    disable_timed_event(&connection->send_heartbeat_event);
    disable_timed_event(&connection->timeout_event);
    return 1;
}

int heartbeat_send_event(void *carry_data) {
    struct timed_event_data *data = carry_data;
    struct rasta_heartbeat_handle *h = (struct rasta_heartbeat_handle *)data->handle;

    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA HEARTBEAT", "send Heartbeat");

    struct rasta_connection *connection = data->connection;

    if (connection == NULL || connection->hb_locked) {
        return 0;
    }

    // connection is valid, check current state
    if (connection->current_state == RASTA_CONNECTION_UP || connection->current_state == RASTA_CONNECTION_RETRREQ || connection->current_state == RASTA_CONNECTION_RETRRUN) {
        sendHeartbeat(connection, 0);

        logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA HEARTBEAT", "Heartbeat sent to %d", connection->remote_id);
    }

    return 0;
}

int data_send_event(void *carry_data) {
    rasta_sending_handle *h = carry_data;

    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA send handler", "send data");

    rasta_connection *con = h->connection;

    unsigned int retransmission_backlog_size = sr_retransmission_queue_item_count(con);
    // Because of this condition, this method does not reliably free up space in the send queue.
    // However, we need to pass on backpressure to the caller...
    if (retransmission_backlog_size <= con->config->retransmission.max_retransmission_queue_size) {
        unsigned int send_backlog_size = sr_send_queue_item_count(con);

        if (send_backlog_size > 0) {
            logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA send handler", "Messages waiting to be sent: %d",
                        send_backlog_size);

            struct RastaMessageData app_messages;
            struct RastaByteArray msg;

            if (send_backlog_size >= h->config->max_packet) {
                send_backlog_size = h->config->max_packet;
            }
            allocateRastaMessageData(&app_messages, send_backlog_size);

            logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA send handler",
                        "Sending %d application messages from queue",
                        send_backlog_size);

            for (unsigned int i = 0; i < send_backlog_size; i++) {

                struct RastaByteArray *elem;
                elem = fifo_pop(con->fifo_send);
                logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA send handler",
                            "Adding application message to data packet");

                allocateRastaByteArray(&msg, elem->length);
                msg.bytes = rmemcpy(msg.bytes, elem->bytes, elem->length);
                freeRastaByteArray(elem);
                rfree(elem);
                app_messages.data_array[i] = msg;
            }

            struct RastaPacket data = createDataMessage(con->remote_id, con->my_id, con->sn_t,
                                                        con->cs_t, cur_timestamp(), con->ts_r,
                                                        app_messages, h->hashing_context);

            struct RastaByteArray packet = rastaModuleToBytes(&data, h->hashing_context);

            struct RastaByteArray *to_fifo = rmalloc(sizeof(struct RastaByteArray));
            allocateRastaByteArray(to_fifo, packet.length);
            rmemcpy(to_fifo->bytes, packet.bytes, packet.length);
            if (!fifo_push(con->fifo_retransmission, to_fifo)) {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA send handler", "discarding packet because retransmission queue is full");
            }

            redundancy_mux_send(con->redundancy_channel, &data, con->role);

            logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA send handler", "Sent data packet from queue");

            con->sn_t = data.sequence_number + 1;

            // set last message ts
            reschedule_event(&con->send_heartbeat_event);

            freeRastaMessageData(&app_messages);
            freeRastaByteArray(&packet);
            freeRastaByteArray(&data.data);
        }
    }

    if (sr_send_queue_item_count(con) == 0) {
        // Disable this event until new data arrives
        disable_timed_event(&h->send_event);
    }

    return 0;
}

void log_main_loop_state(struct rasta_handle *h, event_system *ev_sys, const char *message) {
    int fd_event_count = 0, fd_event_active_count = 0, timed_event_count = 0, timed_event_active_count = 0;
    for (fd_event *ev = ev_sys->fd_events.first; ev; ev = ev->next) {
        fd_event_count++;
        fd_event_active_count += !!ev->enabled;
    }
    for (timed_event *ev = ev_sys->timed_events.first; ev; ev = ev->next) {
        timed_event_count++;
        timed_event_active_count += !!ev->enabled;
    }
    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA EVENT-SYSTEM", "%s | %d/%d fd events and %d/%d timed events active",
               message, fd_event_active_count, fd_event_count, timed_event_active_count, timed_event_count);
}

// HACK
// TODO: Also fill this from kex handlers
struct rasta_connection *handle_conreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Received ConnectionRequest from %d", receivedPacket->sender_id);

    if (connection->current_state == RASTA_CONNECTION_CLOSED || connection->current_state == RASTA_CONNECTION_DOWN) {
        sr_init_connection(connection, RASTA_ROLE_SERVER);

        // initialize seq num
        connection->sn_t = connection->sn_i = receivedPacket->sequence_number;

        logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Using %lu as initial sequence number",
                   (long unsigned int)connection->sn_t);

        connection->current_state = RASTA_CONNECTION_DOWN;

        // check received packet (5.5.2)
        if (!sr_check_packet(connection, connection->logger, &connection->config->sending, receivedPacket, "RaSTA HANDLE: ConnectionRequest")) {
            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Packet is not valid");
            sr_close_connection(connection, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            return connection;
        }

        // received packet is a ConReq -> check version
        struct RastaConnectionData connectionData = extractRastaConnectionData(receivedPacket);

        logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Client has version %.4s", connectionData.version);

        if (compare_version(&RASTA_VERSION, &connectionData.version) == 0 ||
            compare_version(&RASTA_VERSION, &connectionData.version) == -1 ||
            version_accepted(connection->config, &connectionData.version)) {

            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Version accepted");

            // same version, or lower version -> client has to decide -> send ConResp

            // set values according to 5.6.2 [3]
            update_connection_attrs(connection, receivedPacket);
            update_confirmed_attrs(connection, receivedPacket);

            // save N_SENDMAX of partner
            connection->connected_recv_buffer_size = connectionData.send_max;

            connection->t_i = connection->config->sending.t_max;

            unsigned char *version = (unsigned char *)RASTA_VERSION;

            // send ConResp
            struct RastaPacket conresp = createConnectionResponse(connection->remote_id, connection->my_id,
                                                                  connection->sn_t, connection->cs_t,
                                                                  cur_timestamp(), connection->cts_r,
                                                                  connection->config->sending.send_max,
                                                                  version, &connection->redundancy_channel->hashing_context);

            connection->sn_t = connection->sn_t + 1;

            connection->current_state = RASTA_CONNECTION_START;

            fire_on_connection_state_change(sr_create_notification_result(NULL, connection));

            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Send Connection Response - waiting for Heartbeat");

            // Send connection response immediately (don't go through packet batching)
            redundancy_mux_send(connection->redundancy_channel, &conresp, connection->role);

            freeRastaByteArray(&conresp.data);
        } else {
            logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: ConnectionRequest", "Version unacceptable - sending DisconnectionRequest");
            sr_close_connection(connection, RASTA_DISC_REASON_INCOMPATIBLEVERSION, 0);
            return connection;
        }
    } else {
        logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionRequest", "Connection is in invalid state (%d) send DisconnectionRequest", connection->current_state);
        sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
    }
    return connection;
}

struct rasta_connection *handle_conresp(struct rasta_connection *con, struct RastaPacket *receivedPacket) {

    logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Received ConnectionResponse from %d", receivedPacket->sender_id);

    logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Checking packet..");
    if (!sr_check_packet(con, con->logger, &con->config->sending, receivedPacket, "RaSTA HANDLE: ConnectionResponse")) {
        sr_close_connection(con, RASTA_DISC_REASON_PROTOCOLERROR, 0);
        return con;
    }

    if (con->current_state == RASTA_CONNECTION_START) {
        if (con->role == RASTA_ROLE_CLIENT) {
            // handle normal conresp
            logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Current state is in order");

            // correct type of packet received -> version check
            struct RastaConnectionData connectionData = extractRastaConnectionData(receivedPacket);

            // logger_log(&connection->logger, LOG_LEVEL_INFO, "RaSTA open con", "server is running RaSTA version %s", connectionData.version);

            logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Client has version %s", connectionData.version);

            if (version_accepted(con->config, &connectionData.version)) {

                logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Version accepted");

                // same version or accepted versions -> send hb to complete handshake

                // set values according to 5.6.2 [3]
                update_connection_attrs(con, receivedPacket);
                con->cs_r = receivedPacket->confirmed_sequence_number;

                // update state, ready to send data
                con->current_state = RASTA_CONNECTION_UP;

                // send hb
                logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Sending heartbeat..");
                sendHeartbeat(con, 1);

#ifdef ENABLE_OPAQUE
                if (con->config->kex.mode == KEY_EXCHANGE_MODE_OPAQUE) {
                    send_KexRequest(con);
                }
#endif

                // fire connection state changed event
                fire_on_connection_state_change(sr_create_notification_result(NULL, con));
                // fire handshake complete event
                fire_on_handshake_complete(sr_create_notification_result(NULL, con));

                // start sending heartbeats
                enable_timed_event(&con->send_heartbeat_event);

                con->hb_locked = 0;

                // save the N_SENDMAX of remote
                con->connected_recv_buffer_size = connectionData.send_max;

                // arm the timeout timer
                enable_timed_event(&con->timeout_event);

            } else {
                // version not accepted -> disconnect
                logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Version not acceptable - send DisonnectionRequest");
                sr_close_connection(con, RASTA_DISC_REASON_INCOMPATIBLEVERSION, 0);
                return con;
            }
        } else {
            // Server don't receive conresp
            sr_close_connection(con, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);

            logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Server received ConnectionResponse - send Disconnection Request");
            return con;
        }
    } else if (con->current_state == RASTA_CONNECTION_RETRREQ || con->current_state == RASTA_CONNECTION_RETRRUN || con->current_state == RASTA_CONNECTION_UP) {
        sr_close_connection(con, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        logger_log(con->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: ConnectionResponse", "Received ConnectionResponse in wrong state - semd DisconnectionRequest");
        return con;
    }
    return con;
}

// TODO: Move to handlers
int handle_hb(rasta_connection *connection, struct RastaPacket *receivedPacket) {
    logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "Received heartbeat from %d", receivedPacket->sender_id);

    if (connection->current_state == RASTA_CONNECTION_START) {
        // heartbeat is for connection setup
        logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "Establish connection");

        // if SN not in Seq -> disconnect and close connection
        if (!sr_sn_in_seq(connection, receivedPacket)) {
            logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Heartbeat", "Connection HB SN not in Seq");

            if (connection->role == RASTA_ROLE_SERVER) {
                // SN not in Seq
                sr_close_connection(connection, RASTA_DISC_REASON_SEQNERROR, 0);
            } else {
                // Client
                sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
            }
        }

        // if client receives HB in START -> disconnect and close
        if (connection->role == RASTA_ROLE_CLIENT) {
            sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        }

        if (sr_cts_in_seq(connection, &connection->config->sending, receivedPacket)) {
            // set values according to 5.6.2 [3]
            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "Heartbeat is valid connection successful");
            update_connection_attrs(connection, receivedPacket);
            connection->cs_r = receivedPacket->confirmed_sequence_number;

            if (connection->config->kex.mode == KEY_EXCHANGE_MODE_NONE) {
                // sequence number correct, ready to receive data
                connection->current_state = RASTA_CONNECTION_UP;
            } else {
                // need to negotiate session key first
                connection->current_state = RASTA_CONNECTION_KEX_REQ;
            }

            connection->hb_locked = 0;

            // fire connection state changed event
            fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
            // fire handshake complete event
            fire_on_handshake_complete(sr_create_notification_result(NULL, connection));

            // start sending heartbeats
            enable_timed_event(&connection->send_heartbeat_event);

            // arm the timeout timer
            enable_timed_event(&connection->timeout_event);

            connection->is_new = true;

            return 1;
        } else {
            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "Heartbeat is invalid");

            // sequence number check failed -> disconnect
            sr_close_connection(connection, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            return 0;
        }
    }

    if (sr_sn_in_seq(connection, receivedPacket)) {
        logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "SN in SEQ");
        // heartbeats also permissible during key exchange phase, since computation could exceed heartbeat interval
        if (connection->current_state == RASTA_CONNECTION_UP || connection->current_state == RASTA_CONNECTION_RETRRUN ||
            connection->current_state == RASTA_CONNECTION_KEX_REQ || connection->current_state == RASTA_CONNECTION_KEX_RESP ||
            connection->current_state == RASTA_CONNECTION_KEX_AUTH) {
            // check cts_in_seq
            if (sr_cts_in_seq(connection, &connection->config->sending, receivedPacket)) {
                logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "CTS in SEQ");

                updateTimeoutInterval(receivedPacket->confirmed_timestamp, connection, &connection->config->sending);
                updateDiagnostic(connection, receivedPacket, &connection->config->sending);

                // set values according to 5.6.2 [3]
                update_connection_attrs(connection, receivedPacket);
                update_confirmed_attrs(connection, receivedPacket);

                // cs_r updated, remove confirmed messages
                sr_remove_confirmed_messages(connection);

                if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
                    connection->current_state = RASTA_CONNECTION_UP;
                    logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "State changed from RetrRun to Up");
                    fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
                }
            } else {
                logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "CTS not in SEQ - send DisconnectionRequest");
                // close connection
                sr_close_connection(connection, RASTA_DISC_REASON_TIMEOUT, 0);
            }
        }
    } else {
        logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "SN not in SEQ");

        if (connection->current_state == RASTA_CONNECTION_UP || connection->current_state == RASTA_CONNECTION_RETRRUN) {
            // ignore message, send RetrReq and goto state RetrReq
            // TODO:send retransmission
            // send_retrreq(con);
            connection->current_state = RASTA_CONNECTION_RETRREQ;
            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Heartbeat", "Send retransmission");

            // fire connection state changed event
            fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
        }
    }
    return 0;
}

// TODO: This should be moved into safety_retransmission, and not be called from the outside. Instead, add rasta_connect.
struct rasta_connection* sr_connect(struct rasta_handle *h, unsigned long id) {
    rasta_connection *connection = NULL;

    for (unsigned i = 0; i < h->rasta_connections_length; i++) {
        if (h->rasta_connections[i].remote_id == id) {
            connection = &h->rasta_connections[i];
            break;
        }
    }

    if (connection == NULL || redundancy_mux_connect_channel(connection, &h->mux, connection->redundancy_channel) != 0) {
        return NULL;
    }

    sr_init_connection(connection, RASTA_ROLE_CLIENT);

    // initialize seq nums and timestamps
    connection->sn_t = h->config->initial_sequence_number;

    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA CONNECT", "Using %lu as initial sequence number",
               (long unsigned int)connection->sn_t);

    connection->cs_t = 0;
    connection->cts_r = cur_timestamp();
    connection->t_i = h->config->sending.t_max;

    unsigned char *version = (unsigned char *)RASTA_VERSION;

    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA CONNECT", "Local version is %s", version);

    // send ConReq
    struct RastaPacket conreq = createConnectionRequest(connection->remote_id, connection->my_id,
                                                        connection->sn_t, cur_timestamp(),
                                                        h->config->sending.send_max,
                                                        version, &connection->redundancy_channel->hashing_context);
    connection->sn_i = connection->sn_t;

    // Send connection request immediately (don't go through packet batching)
    redundancy_mux_send(connection->redundancy_channel, &conreq, connection->role);

    // increase sequence number
    connection->sn_t++;

    // update state
    connection->current_state = RASTA_CONNECTION_START;

    freeRastaByteArray(&conreq.data);

    // fire connection state changed event
    fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
    // Wait for connection response

    enable_timed_event(&connection->handshake_timeout_event);

    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA CONNECT", "awaiting connection response from %d", connection->remote_id);
    log_main_loop_state(h, h->ev_sys, "event-system started");
    event_system_start(h->ev_sys);

    disable_timed_event(&connection->handshake_timeout_event);

    // What happened? Timeout, or user abort, or success?
    if (connection->current_state != RASTA_CONNECTION_UP) {
        redundancy_mux_close_channel(connection->redundancy_channel);
        return NULL;
    }

    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA CONNECT", "handshake completed with %d", connection->remote_id);

    return connection;
}

int rasta_recv(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len) {
    struct rasta_handle *h = &user_configuration->h;
    event_system *event_system = &user_configuration->rasta_lib_event_system;

    while (connection->current_state == RASTA_CONNECTION_UP && sr_recv_queue_item_count(connection) == 0) {
        log_main_loop_state(h, event_system, "event-system started");
        event_system_start(event_system);
    }

    if (connection->current_state != RASTA_CONNECTION_UP) {
        // TODO: If sockets are broken, their event handlers have to be removed...
        return -1;
    }

    struct RastaByteArray *elem;
    elem = fifo_pop(connection->fifo_receive);
    size_t received_len = (len < elem->length) ? len : elem->length;

    if (len < elem->length) {
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", 
            "supplied buffer (%ld bytes) is smaller than message length (%d bytes) - received message may be incomplete!", len, elem->length);
    }

    rmemcpy(buf, elem->bytes, received_len);
    freeRastaByteArray(elem);
    rfree(elem);

    return received_len;
}

int rasta_send(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len) {
    struct RastaMessageData messageData1;
    allocateRastaMessageData(&messageData1, 1);
    messageData1.data_array[0].bytes = buf;
    messageData1.data_array[0].length = len;

    sr_send(&user_configuration->h, connection, messageData1);
    rfree(messageData1.data_array);
    return 0;
}

void rasta_bind(struct rasta_handle *h) {
    redundancy_mux_bind(h);
}

struct rasta_connection * rasta_accept(rasta_lib_configuration_t user_configuration) {
    struct rasta_handle *h = &user_configuration->h;
    event_system *event_system = &user_configuration->rasta_lib_event_system;

    // accept events were already prepared by sr_listen
    // event system will break when we have received the first heartbeat of a new connection
    log_main_loop_state(h, event_system, "event-system started");
    event_system_start(event_system);

    for (unsigned i = 0; i < h->rasta_connections_length; i++) {
        if (h->rasta_connections[i].is_new) {
            h->rasta_connections[i].is_new = false;
            return &h->rasta_connections[i];
        }
    }

    return NULL;
}
