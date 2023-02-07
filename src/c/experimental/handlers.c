#include "handlers.h"

#include <stdlib.h>

#include "../retransmission/safety_retransmission.h"
#include "../retransmission/protocol.h"

/**
 * processes a received Key Exchange Request packet
 * @param con the used connection
 * @param packet the received Key Exchange Request packet
 */
void handle_kex_request(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Key Exchange Request", "received Data");

    if (sr_sn_in_seq(connection, receivedPacket)) {
        if (connection->current_state != RASTA_CONNECTION_KEX_REQ && (!h->handle->config.kex.rekeying_interval_ms || connection->current_state != RASTA_CONNECTION_UP)) {
            // received Key Exchange Request in the wrong phase -> disconnect and close
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else {
            // sn_in_seq == true -> check cts_in_seq

            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Req", "SN in SEQ");

            if (sr_cts_in_seq(connection, h->config, receivedPacket)) {
                if (h->handle->config.kex.mode == KEY_EXCHANGE_MODE_NONE) {
                    logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Req", "Key exchange request received when not activated!");
                    sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
                    return;
                }
#ifdef ENABLE_OPAQUE

                struct RastaPacket response;
                connection->kex_state.last_key_exchanged_millis = get_current_time_ms();

                if (connection->kex_state.last_key_exchanged_millis) {
                    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Resp", "Accepted rekeying at %" PRIu64, connection->kex_state.last_key_exchanged_millis);
                }

                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Req", "CTS in SEQ");

                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Req", "Key exchange request received at %" PRIu64, connection->kex_state.last_key_exchanged_millis);
                // valid Key Exchange request packet received

                response = createKexResponse(connection->remote_id, connection->my_id, connection->sn_t,
                                             receivedPacket.sequence_number, current_ts(),
                                             receivedPacket.timestamp, h->hashing_context, h->handle->config.kex.psk,
                                             (uint8_t *)receivedPacket.data.bytes, receivedPacket.data.length, connection->sn_i,
                                             &connection->kex_state,
                                             &h->handle->config.kex, h->logger);

                redundancy_mux_send(h->mux, response);

                // set values according to 5.6.2 [3]
                connection->sn_r = receivedPacket.sequence_number + 1;
                connection->sn_t += 1;
                connection->cs_t = receivedPacket.sequence_number;
                connection->cs_r = receivedPacket.confirmed_sequence_number;
                connection->ts_r = receivedPacket.timestamp;
                connection->cts_r = receivedPacket.confirmed_timestamp;
                // con->cts_r = current_timestamp();

                // cs_r updated, remove confirmed messages
                sr_remove_confirmed_messages(h, connection);

                // wait for client to send auth packet, indicating that on the client's side, the exchange worked
                connection->current_state = RASTA_CONNECTION_KEX_AUTH;

                logger_hexdump(h->logger, LOG_LEVEL_INFO, connection->kex_state.session_key, sizeof(connection->kex_state.session_key), "Setting hash key to:");

                rasta_set_hash_key_variable(h->hashing_context, (char *)connection->kex_state.session_key, sizeof(connection->kex_state.session_key));

#else
                logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Req", "Not implemented!");

                abort();
#endif

            } else {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Kex", "CTS not in SEQ");

                // increase cs error counter
                connection->errors.cs++;

                // send DiscReq and close connection
                sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        }
    } else {
        // received key exchange request in phase during which I should not have received one -> disconnect and close
        sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
    }
}

/**
 * processes a received Key Exchange Response packet
 * @param con the used connection
 * @param packet the received Key Exchange Response packet
 */
void handle_kex_response(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Key Exchange Response", "received Data");

    if (sr_sn_in_seq(connection, receivedPacket)) {
        if (connection->current_state != RASTA_CONNECTION_KEX_RESP) {
            // received Key Exchange Response in the wrong phase -> disconnect and close
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else {
            // sn_in_seq == true -> check cts_in_seq
            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Resp", "SN in SEQ");

            if (sr_cts_in_seq(connection, h->config, receivedPacket)) {
                if (h->handle->config.kex.mode == KEY_EXCHANGE_MODE_NONE) {
                    logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Resp", "Key exchange response received when not activated!");
                    sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
                    return;
                }
#ifdef ENABLE_OPAQUE

                struct RastaPacket response;
                int ret;

                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Resp", "CTS in SEQ");
                // valid Key Exchange response packet received
                ret = kex_recover_credential(&connection->kex_state, (const uint8_t *)receivedPacket.data.bytes, receivedPacket.data.length, connection->my_id, connection->remote_id, connection->sn_i, h->logger);

                if (ret) {
                    logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Resp", "Could not recover credentials!");
                    sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
                    return;
                }
                response = createKexAuthentication(connection->remote_id, connection->my_id, connection->sn_t, receivedPacket.sequence_number, current_ts(), receivedPacket.timestamp, h->hashing_context, connection->kex_state.user_auth_server, sizeof(connection->kex_state.user_auth_server), h->logger);

                redundancy_mux_send(h->mux, response);

                // set values according to 5.6.2 [3]
                connection->sn_r = receivedPacket.sequence_number + 1;
                connection->sn_t += 1;
                connection->cs_t = receivedPacket.sequence_number;
                connection->cs_r = receivedPacket.confirmed_sequence_number;
                connection->ts_r = receivedPacket.timestamp;
                connection->cts_r = receivedPacket.confirmed_timestamp;
                // con->cts_r = current_timestamp();

                // cs_r updated, remove confirmed messages
                sr_remove_confirmed_messages(h, connection);

                // kex is done from our PoV, can expect data from now
                connection->current_state = RASTA_CONNECTION_UP;

                logger_hexdump(h->logger, LOG_LEVEL_INFO, connection->kex_state.session_key, sizeof(connection->kex_state.session_key), "Setting hash key to:");

                rasta_set_hash_key_variable(h->hashing_context, (char *)connection->kex_state.session_key, sizeof(connection->kex_state.session_key));
#else
                logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Resp", "Not implemented!");
                abort();
#endif

            } else {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Kex", "CTS not in SEQ");

                // increase cs error counter
                connection->errors.cs++;

                // send DiscReq and close connection
                sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        }
    } else {
        // received key exchange response in phase during which I should not have received one -> disconnect and close
        sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
    }
}

/**
 * processes a received Key Exchange Authentication packet
 * @param con the used connection
 * @param packet the received Key Exchange Authentication packet
 */
void handle_kex_auth(struct rasta_receive_handle *h, struct rasta_connection *connection, struct RastaPacket receivedPacket) {
    logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Key Exchange Authentication", "received Data");

    if (sr_sn_in_seq(connection, receivedPacket)) {
        if (connection->current_state != RASTA_CONNECTION_KEX_AUTH) {
            // received Key Exchange Authentication in the wrong phase -> disconnect and close
            sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
        } else {
            // sn_in_seq == true -> check cts_in_seq

            logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Auth", "SN in SEQ");

            if (sr_cts_in_seq(connection, h->config, receivedPacket)) {

                if (h->handle->config.kex.mode == KEY_EXCHANGE_MODE_NONE) {
                    logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Auth", "Key exchange Authentication received when not activated!");
                    sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
                    return;
                }
#ifdef ENABLE_OPAQUE
                int ret;
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: KEX Auth", "CTS in SEQ");
                // valid Key Exchange Authentication packet received
                ret = kex_authenticate_user(&connection->kex_state, (const uint8_t *)receivedPacket.data.bytes, receivedPacket.data.length, h->logger);

                if (ret) {
                    logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Auth", "Could not authenticate user");
                    sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
                    return;
                }

                // set values according to 5.6.2 [3]
                connection->sn_r = receivedPacket.sequence_number + 1;
                connection->cs_t = receivedPacket.sequence_number;
                connection->cs_r = receivedPacket.confirmed_sequence_number;
                connection->ts_r = receivedPacket.timestamp;
                connection->cts_r = receivedPacket.confirmed_timestamp;
                // con->cts_r = current_timestamp();

                // cs_r updated, remove confirmed messages
                sr_remove_confirmed_messages(h, connection);

                // kex is done from our PoV, can expect data from now
                connection->current_state = RASTA_CONNECTION_UP;
#else
                logger_log(h->logger, LOG_LEVEL_ERROR, "RaSTA HANDLE: KEX Auth", "Not implemented!");
                abort();
#endif

            } else {
                logger_log(h->logger, LOG_LEVEL_INFO, "RaSTA HANDLE: Kex", "CTS not in SEQ");

                // increase cs error counter
                connection->errors.cs++;

                // send DiscReq and close connection
                sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_PROTOCOLERROR, 0);
            }
        }
    } else {
        // received key exchange response in phase during which I should not have received one -> disconnect and close
        sr_close_connection(connection, h->handle, h->mux, h->info, RASTA_DISC_REASON_UNEXPECTEDTYPE, 0);
    }
}
