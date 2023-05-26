#include "events.h"

#include <rasta/logging.h>
#include <rasta/rmemory.h>
#include <rasta/rastahandle.h>
#include <rasta/rastaredundancy.h>

#include "transport.h"
#include "diagnostics.h"

int channel_accept_event(void *carry_data) {
    struct accept_event_data *data = carry_data;

    logger_log(data->h->mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux accept", "Socket ready to accept");

    struct sockaddr_in addr;
    int fd = transport_accept(data->socket, &addr);

    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, str, INET_ADDRSTRLEN);

    // Find the suitable transport channel in the mux
    rasta_transport_channel * channel = NULL;
    for (unsigned i = 0; i < data->h->mux.redundancy_channels_count; i++) {
        for (unsigned j = 0; j < data->h->mux.redundancy_channels[i].transport_channel_count; j++) {
            rasta_transport_channel *current_channel = &data->h->mux.redundancy_channels[i].transport_channels[j];
            if (strncmp(current_channel->remote_ip_address, str, INET_ADDRSTRLEN) == 0
                && current_channel->remote_port == ntohs(addr.sin_port)) {
                channel = current_channel;
                break;
            }
        }
    }

    if (channel != NULL) {
        channel->file_descriptor = fd;
        // We may not forget this:
        channel->receive_event.fd = fd;
        channel->tls_mode = data->socket->tls_mode;
        channel->connected = true;
#ifdef ENABLE_TLS
        channel->tls_state = RASTA_TLS_CONNECTION_READY;
        channel->ctx = data->socket->ctx;
        channel->ssl = data->socket->ssl;
#endif

        if (channel->receive_event.callback != NULL) {
            enable_fd_event(&channel->receive_event);
        }
    } else {
        logger_log(data->h->mux.logger, LOG_LEVEL_INFO, "RaSTA RedMux accept", "Rejecting connection from unknown peer %s:%u", str, ntohs(addr.sin_port));
        close(fd);
    }

    return 0;
}

int channel_receive_event(void *carry_data) {
    struct receive_event_data *data = carry_data;
    rasta_connection * connection = data->connection;

    unsigned char buffer[MAX_DEFER_QUEUE_MSG_SIZE] = {0};
    struct sockaddr_in sender = {0};

    // when performing DTLS accept, len = 0 doesn't signal a broken connection
    // ifdef needed because UDP/TCP do not know about the tls_state
#ifdef ENABLE_TLS
    bool is_dtls_conn_ready = data->socket->tls_mode == TLS_MODE_DTLS_1_2 && data->socket->tls_state == RASTA_TLS_CONNECTION_READY;
#else
    bool is_dtls_conn_ready = false;
#endif
    
    ssize_t len = receive_callback(data, buffer, &sender);

    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender.sin_addr, str, INET_ADDRSTRLEN);

    rasta_transport_channel *transport_channel = data->channel;

    if (transport_channel == NULL) {
        // We will only enter this branch for UDP and DTLS

        // Find the suitable transport channel in the mux
        for (unsigned i = 0; i < data->h->mux.redundancy_channels_count; i++) {
            for (unsigned j = 0; j < data->h->mux.redundancy_channels[i].transport_channel_count; j++) {
                rasta_transport_channel *current_channel = &data->h->mux.redundancy_channels[i].transport_channels[j];
                if (strncmp(current_channel->remote_ip_address, str, INET_ADDRSTRLEN) == 0
                    && current_channel->remote_port == ntohs(sender.sin_port)) {
                    transport_channel = current_channel;
                    connection = &data->h->rasta_connections[i];
                    break;
                }
            }
        }

        if (transport_channel == NULL) {
            // Ignore and continue
            logger_log(data->connection->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "Discarding packet from unknown peer %s:%u", str, ntohs(sender.sin_port));
            return 0;
        }

        transport_channel->file_descriptor = data->socket->file_descriptor;

        // We can regard UDP channels as 'always connected' (no re-dial possible)
        transport_channel->connected = true;
    }

    run_channel_diagnostics(connection->redundancy_channel, transport_channel->id);

    logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "Channel %d calling receive", transport_channel->id);

    if (len <= 0 && !is_dtls_conn_ready) {
        // Connection is broken
        // TODO: What about disabling events?
        transport_channel->connected = false;

        // Why should a readable UDP socket return len <= 0?
        // if (data->socket != NULL) {
        //     disable_fd_event(&data->socket->receive_event);
        //     remove_fd_event(data->h->ev_sys, &data->socket->receive_event);
        // }
    }

    if (len < 0) {
        // Peer has not performed an orderly shutdown
        // TODO: If this is a RaSTA client, try to re-establish a connection somewhere else
        // transport_redial(transport_channel);
    }

    if (len <= 0 && !is_dtls_conn_ready) {
        if (connection != NULL) {
            return handle_closed_transport(connection, connection->redundancy_channel);
        }
        // Ignore and continue
        return 0;
    }

    int result = receive_packet(connection->redundancy_channel->mux, transport_channel, buffer, len);

    if (result) {
        // Deliver messages to the upper layer
        return red_f_deliverDeferQueue(connection, connection->redundancy_channel);
    }

    logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Channel %d receive done",
               transport_channel->id);
    return !!result;
}
