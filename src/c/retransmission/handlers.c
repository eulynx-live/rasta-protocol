#include "handlers.h"
#include "safety_retransmission.h"
#include "protocol.h"

void handle_discreq(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: DisconnectionRequest", "received DiscReq");

    connection->current_state = RASTA_CONNECTION_CLOSED;
    sr_reset_connection(connection, connection->remote_id, h->info);

    // remove redundancy channel
    redundancy_mux_remove_channel(h->mux, connection->remote_id);

    // struct RastaDisconnectionData disconnectionData = extractRastaDisconnectionData(receivedPacket);

    // fire connection tls_state changed event
    fire_on_connection_state_change(sr_create_notification_result(h->handle, connection));
    // fire disconnection request received event
    struct RastaDisconnectionData data = extractRastaDisconnectionData(receivedPacket);
    fire_on_discrequest_state_change(sr_create_notification_result(h->handle, connection), data);
}

/**
 * processes a received Data packet
 * @param con the used connection
 * @param packet the received data packet
 */
void handle_data(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "received Data");

    if (sr_sn_in_seq(connection, receivedPacket)) {
        if (connection->current_state == RASTA_CONNECTION_START || connection->current_state == RASTA_CONNECTION_KEX_REQ || connection->current_state == RASTA_CONNECTION_KEX_RESP || connection->current_state == RASTA_CONNECTION_KEX_AUTH) {
            // received data in START or when key exchange still in progress-> disconnect and close
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_UP) {
            // sn_in_seq == true -> check cts_in_seq

            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "SN in SEQ");

            if (sr_cts_in_seq(connection, h->config, receivedPacket)) {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "CTS in SEQ");
                // valid data packet received
                // read application messages and push into queue
                sr_add_app_messages_to_buffer(h, connection, receivedPacket);

                // set values according to 5.6.2 [3]
                connection->sn_r = receivedPacket.sequence_number + 1;
                connection->cs_t = receivedPacket.sequence_number;
                connection->cs_r = receivedPacket.confirmed_sequence_number;
                connection->ts_r = receivedPacket.timestamp;
                connection->cts_r = receivedPacket.confirmed_timestamp;
                // con->cts_r = current_timestamp();

                // cs_r updated, remove confirmed messages
                sr_remove_confirmed_messages(h, connection);

            } else {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "CTS not in SEQ");

                // increase cs error counter
                connection->errors.cs++;

                // send DiscReq and close connection
                sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            if (sr_cts_in_seq(connection, h->config, receivedPacket)) {
                // set values according to 5.6.2 [3]
                connection->sn_r = receivedPacket.sequence_number + 1;
                connection->cs_t = receivedPacket.sequence_number;
                connection->ts_r = receivedPacket.timestamp;
            } else {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "retransmission failed, disconnect and close");
                // retransmission failed, disconnect and close
                sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        }
    } else {

        if (connection->current_state == RASTA_CONNECTION_START) {
            // received data in START -> disconnect and close
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN || connection->current_state == RASTA_CONNECTION_UP) {
            // increase SN error counter
            connection->errors.sn++;

            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "send retransmission request");
            // send RetrReq
            sendRetransmissionRequest(h->mux, connection);

            // change tls_state to RetrReq
            connection->current_state = RASTA_CONNECTION_RETRREQ;

            fire_on_connection_state_change(sr_create_notification_result(h->handle, connection));
        } else if (connection->current_state == RASTA_CONNECTION_RETRREQ) {
            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "package is ignored - waiting for RETRResponse");
        }
    }
}

/**
 * processes a received RetrReq packet
 * @param con the used connection
 * @param packet the received RetrReq packet
 */
void handle_retrreq(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "received RetrReq");

    if (sr_sn_in_seq(connection, receivedPacket)) {
        // sn_in_seq == true
        logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "RetrReq SNinSeq");

        if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "RetrReq: got RetrReq packet in RetrRun mode. closing connection.");

            // send DiscReq to client
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_RETRFAILED, 0);
            // printf("Connection closed / DiscReq sent!\n");
        }

        // FIXME: update connection attr (copy from RASTA_TYPE_DATA case, DRY)
        // set values according to 5.6.2 [3]
        connection->sn_r = receivedPacket.sequence_number + 1;
        connection->cs_t = receivedPacket.sequence_number;
        connection->cs_r = receivedPacket.confirmed_sequence_number;
        connection->ts_r = receivedPacket.timestamp;

        // con->cts_r = current_timestamp();

        // cs_r updated, remove confirmed messages
        sr_remove_confirmed_messages(h, connection);

        // send retransmission response
        sendRetransmissionResponse(h->mux, connection);
        logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "send RetrRes");

        sr_retransmit_data(h, connection);

        if (connection->current_state == RASTA_CONNECTION_UP) {
            // change tls_state to up
            connection->current_state = RASTA_CONNECTION_UP;
        } else if (connection->current_state == RASTA_CONNECTION_RETRREQ) {
            // change tls_state to RetrReq
            connection->current_state = RASTA_CONNECTION_RETRREQ;
        }

        // fire connection tls_state changed event
        fire_on_connection_state_change(sr_create_notification_result(h->handle, connection));
    } else {
        // sn_in_seq == false
        connection->cs_r = receivedPacket.confirmed_sequence_number;

        // cs_r updated, remove confirmed messages
        sr_remove_confirmed_messages(h, connection);

        // send retransmission response
        sendRetransmissionResponse(h->mux, connection);
        logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "send RetrRes");

        sr_retransmit_data(h, connection);
        // change tls_state to RetrReq
        connection->current_state = RASTA_CONNECTION_RETRREQ;

        // fire connection tls_state changed event
        fire_on_connection_state_change(sr_create_notification_result(h->handle, connection));
    }
}

/**
 * processes a received RetrResp packet
 * @param con the used connection
 * @param packet the received RetrResp packet
 */
void handle_retrresp(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    if (connection->current_state == RASTA_CONNECTION_RETRREQ) {
        logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "starting receive retransmitted data");
        // check cts_in_seq
        logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA receive", "RetrResp: CTS in Seq");

        // change to retransmission tls_state
        connection->current_state = RASTA_CONNECTION_RETRRUN;

        // set values according to 5.6.2 [3]
        connection->sn_r = receivedPacket.sequence_number + 1;
        connection->cs_t = receivedPacket.sequence_number;
        connection->cs_r = receivedPacket.confirmed_sequence_number;
        connection->ts_r = receivedPacket.timestamp;

        connection->cts_r = receivedPacket.confirmed_timestamp;
    } else {
        logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA receive", "received packet type retr_resp, but not in tls_state retr_req");
        sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
    }
}

/**
 * processes a received RetrData packet
 * @param con the used connection
 * @param packet the received data packet
 */
void handle_retrdata(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    if (sr_sn_in_seq(connection, receivedPacket)) {

        if (connection->current_state == RASTA_CONNECTION_UP) {
            // close connection
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            if (!sr_cts_in_seq(connection, h->config, receivedPacket)) {
                // cts not in seq -> close connection
                sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            } else {
                // cts is in seq -> add data to receive buffer
                logger_log(h->logger, LOG_LEVEL_DEBUG, "Process RetrData", "CTS in seq, adding messages to buffer");
                sr_add_app_messages_to_buffer(h, connection, receivedPacket);

                // set values according to 5.6.2 [3]
                connection->sn_r = receivedPacket.sequence_number + 1;
                connection->cs_t = receivedPacket.sequence_number;
                connection->cs_r = receivedPacket.confirmed_sequence_number;
                connection->ts_r = receivedPacket.timestamp;
            }
        }
    } else {
        // sn not in seq
        logger_log(h->logger, LOG_LEVEL_DEBUG, "Process RetrData", "SN not in Seq");
        logger_log(h->logger, LOG_LEVEL_DEBUG, "Process RetrData", "SN_PDU=%lu, SN_R=%lu",
                   (long unsigned int)receivedPacket.sequence_number, (long unsigned int)connection->sn_r);
        if (connection->current_state == RASTA_CONNECTION_UP) {
            // close connection
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            // send RetrReq
            logger_log(h->logger, LOG_LEVEL_DEBUG, "Process RetrData", "changing to tls_state RetrReq");
            sendRetransmissionRequest(h->mux, connection);
            connection->current_state = RASTA_CONNECTION_RETRREQ;
            fire_on_connection_state_change(sr_create_notification_result(h->handle, connection));
        }
    }
}
