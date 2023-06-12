#include "handlers.h"
#include "safety_retransmission.h"
#include "protocol.h"

void update_connection_attrs(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    connection->sn_r = receivedPacket->sequence_number + 1;
    connection->cs_t = receivedPacket->sequence_number;
    connection->ts_r = receivedPacket->timestamp;
}

void update_confirmed_attrs(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    connection->cts_r = receivedPacket->confirmed_timestamp;
    connection->cs_r = receivedPacket->confirmed_sequence_number;
}

int handle_discreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: DisconnectionRequest", "received DiscReq");

    connection->current_state = RASTA_CONNECTION_CLOSED;
    sr_reset_connection(connection);

    // remove redundancy channel
    redundancy_mux_close_channel(connection->redundancy_channel);

    // fire connection state changed event
    fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
    // fire disconnection request received event
    struct RastaDisconnectionData data = extractRastaDisconnectionData(receivedPacket);
    fire_on_discrequest_state_change(sr_create_notification_result(NULL, connection), data);

    return 0;
}

/**
 * processes a received Data packet
 * @param con the used connection
 * @param packet the received data packet
 */
int handle_data(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Data", "received Data");

    int result = 0;

    if (sr_sn_in_seq(connection, receivedPacket)) {
        if (connection->current_state == RASTA_CONNECTION_START || connection->current_state == RASTA_CONNECTION_KEX_REQ || connection->current_state == RASTA_CONNECTION_KEX_RESP || connection->current_state == RASTA_CONNECTION_KEX_AUTH) {
            // received data in START or when key exchange still in progress-> disconnect and close
            sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_UP) {
            // sn_in_seq == true -> check cts_in_seq

            logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Data", "SN in SEQ");

            if (sr_cts_in_seq(connection, &connection->config->sending, receivedPacket)) {
                logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE: Data", "CTS in SEQ");

                // valid data packet received
                // read application messages and push into queue
                sr_add_app_messages_to_buffer(connection, receivedPacket);
                result = 1;

                // set values according to 5.6.2 [3]
                update_connection_attrs(connection, receivedPacket);
                update_confirmed_attrs(connection, receivedPacket);

                // cs_r updated, remove confirmed messages
                sr_remove_confirmed_messages(connection);

            } else {
                logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "CTS not in SEQ");

                // increase cs error counter
                connection->errors.cs++;

                // send DiscReq and close connection
                sr_close_connection(connection, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            if (sr_cts_in_seq(connection, &connection->config->sending, receivedPacket)) {
                // set values according to 5.6.2 [3]
                update_connection_attrs(connection, receivedPacket);
            } else {
                logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "retransmission failed, disconnect and close");
                // retransmission failed, disconnect and close
                sr_close_connection(connection, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        }
    } else {

        if (connection->current_state == RASTA_CONNECTION_START) {
            // received data in START -> disconnect and close
            sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN || connection->current_state == RASTA_CONNECTION_UP) {
            // increase SN error counter
            connection->errors.sn++;

            logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "send retransmission request");
            // send RetrReq
            sendRetransmissionRequest(connection);

            // change state to RetrReq
            connection->current_state = RASTA_CONNECTION_RETRREQ;

            fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
        } else if (connection->current_state == RASTA_CONNECTION_RETRREQ) {
            logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Data", "package is ignored - waiting for RETRResponse");
        }
    }

    return result;
}

/**
 * processes a received RetrReq packet
 * @param con the used connection
 * @param packet the received RetrReq packet
 */
int handle_retrreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "received RetrReq");

    if (sr_sn_in_seq(connection, receivedPacket)) {
        // sn_in_seq == true
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "RetrReq SNinSeq");

        if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "RetrReq: got RetrReq packet in RetrRun mode. closing connection.");

            // send DiscReq to client
            sr_close_connection(connection, RASTA_DISC_REASON_RETRFAILED, 0);
            // printf("Connection closed / DiscReq sent!\n");
        }

        // set values according to 5.6.2 [3]
        update_connection_attrs(connection, receivedPacket);
        connection->cs_r = receivedPacket->confirmed_sequence_number;

        // cs_r updated, remove confirmed messages
        sr_remove_confirmed_messages(connection);

        // send retransmission response
        sendRetransmissionResponse(connection);
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "send RetrRes");

        sr_retransmit_data(connection);

        if (connection->current_state == RASTA_CONNECTION_UP) {
            // change state to up
            connection->current_state = RASTA_CONNECTION_UP;
        } else if (connection->current_state == RASTA_CONNECTION_RETRREQ) {
            // change state to RetrReq
            connection->current_state = RASTA_CONNECTION_RETRREQ;
        }

        // fire connection state changed event
        fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
    } else {
        // sn_in_seq == false
        connection->cs_r = receivedPacket->confirmed_sequence_number;

        // cs_r updated, remove confirmed messages
        sr_remove_confirmed_messages(connection);

        // send retransmission response
        sendRetransmissionResponse(connection);
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "send RetrRes");

        sr_retransmit_data(connection);
        // change state to RetrReq
        connection->current_state = RASTA_CONNECTION_RETRREQ;

        // fire connection state changed event
        fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
    }

    return 0;
}

/**
 * processes a received RetrResp packet
 * @param con the used connection
 * @param packet the received RetrResp packet
 */
int handle_retrresp(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    if (connection->current_state == RASTA_CONNECTION_RETRREQ) {
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "starting receive retransmitted data");
        // check cts_in_seq
        logger_log(connection->logger, LOG_LEVEL_INFO, "RaSTA receive", "RetrResp: CTS in Seq");

        // change to retransmission state
        connection->current_state = RASTA_CONNECTION_RETRRUN;

        // set values according to 5.6.2 [3]
        update_connection_attrs(connection, receivedPacket);
        update_confirmed_attrs(connection, receivedPacket);
    } else {
        logger_log(connection->logger, LOG_LEVEL_ERROR, "RaSTA receive", "received packet type retr_resp, but not in state retr_req");
        sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
    }

    return 0;
}

/**
 * processes a received RetrData packet
 * @param con the used connection
 * @param packet the received data packet
 */
int handle_retrdata(struct rasta_connection *connection, struct RastaPacket *receivedPacket) {
    int result = 0;

    if (sr_sn_in_seq(connection, receivedPacket)) {

        if (connection->current_state == RASTA_CONNECTION_UP) {
            // close connection
            sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            if (!sr_cts_in_seq(connection, &connection->config->sending, receivedPacket)) {
                // cts not in seq -> close connection
                sr_close_connection(connection, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            } else {
                // cts is in seq -> add data to receive buffer
                logger_log(connection->logger, LOG_LEVEL_DEBUG, "Process RetrData", "CTS in seq, adding messages to buffer");
                sr_add_app_messages_to_buffer(connection, receivedPacket);
                result = 1;

                // set values according to 5.6.2 [3]
                update_connection_attrs(connection, receivedPacket);
                connection->cs_r = receivedPacket->confirmed_sequence_number;
            }
        }
    } else {
        // sn not in seq
        logger_log(connection->logger, LOG_LEVEL_DEBUG, "Process RetrData", "SN not in Seq");
        logger_log(connection->logger, LOG_LEVEL_DEBUG, "Process RetrData", "SN_PDU=%lu, SN_R=%lu",
                   (long unsigned int)receivedPacket->sequence_number, (long unsigned int)connection->sn_r);
        if (connection->current_state == RASTA_CONNECTION_UP) {
            // close connection
            sr_close_connection(connection, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else if (connection->current_state == RASTA_CONNECTION_RETRRUN) {
            // send RetrReq
            logger_log(connection->logger, LOG_LEVEL_DEBUG, "Process RetrData", "changing to state RetrReq");
            sendRetransmissionRequest(connection);
            connection->current_state = RASTA_CONNECTION_RETRREQ;
            fire_on_connection_state_change(sr_create_notification_result(NULL, connection));
        }
    }

    return result;
}
