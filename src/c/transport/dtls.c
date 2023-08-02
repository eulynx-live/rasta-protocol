
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

#include <rasta/rmemory.h>

#include "bsd_utils.h"
#include "ssl_utils.h"
#include "udp.h"

static void get_client_addr_from_socket(const rasta_transport_socket *transport_socket, struct sockaddr_in *client_addr, socklen_t *addr_len) {
    ssize_t received_bytes;
    char buffer;
    // wait for the first byte of the DTLS Client hello to identify the prospective client
    received_bytes = recvfrom(transport_socket->file_descriptor, &buffer, sizeof(buffer), MSG_PEEK,
                              (struct sockaddr *)client_addr, addr_len);

    if (received_bytes < 0) {
        perror("No clients waiting to connect");
        abort();
    }
}

static void wolfssl_accept(rasta_transport_socket *transport_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // need to open UDP "connection" and accept client before the remaining methods (send / receive) work as expected by RaSTA

    get_client_addr_from_socket(transport_socket, &client_addr, &addr_len);
    // we have received a client hello and can now accept the connection

    if (connect(transport_socket->file_descriptor, (struct sockaddr *)&client_addr, sizeof(client_addr)) != 0) {
        perror("Could not connect to client");
        abort();
    }

    if (wolfSSL_accept(transport_socket->ssl) != SSL_SUCCESS) {

        int e = wolfSSL_get_error(transport_socket->ssl, 0);

        fprintf(stderr, "WolfSSL could not accept connection: %s\n", wolfSSL_ERR_reason_error_string(e));
        abort();
    }

    tls_pin_certificate(transport_socket->ssl, transport_socket->tls_config->peer_tls_cert_path);

    set_dtls_async(transport_socket);
}

static size_t wolfssl_receive_dtls(rasta_transport_socket *transport_socket, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender) {
    int receive_len, received_total = 0;
    socklen_t sender_size = sizeof(*sender);
    struct receive_event_data *data = &transport_socket->receive_event_data;

    get_client_addr_from_socket(transport_socket, sender, &sender_size);

    int red_channel_idx, transport_channel_idx;
    rasta_transport_channel *channel = NULL;

    // find the transport channel corresponding to this socket
    find_channel_by_ip_address(data->h, *sender, &red_channel_idx, &transport_channel_idx);
    if (red_channel_idx != -1 && transport_channel_idx != -1) {
        channel = &data->h->mux.redundancy_channels[red_channel_idx].transport_channels[transport_channel_idx];
    }

    // If this is a client and the channel was connected using udp_send, we may not have
    // told the socket about it. In this case, propagate the connection information to the
    // socket here:
    if (channel != NULL && channel->tls_state == RASTA_TLS_CONNECTION_ESTABLISHED) {
        transport_socket->tls_state = RASTA_TLS_CONNECTION_ESTABLISHED;
        transport_socket->ssl = channel->ssl;
    }

    if (transport_socket->tls_state == RASTA_TLS_CONNECTION_READY) {
        wolfssl_accept(transport_socket);
        transport_socket->tls_state = RASTA_TLS_CONNECTION_ESTABLISHED;

        // propagate our TLS config to the channel
        if (channel != NULL) {
            channel->tls_state = transport_socket->tls_state;
            channel->tls_config = transport_socket->tls_config;
            channel->ssl = transport_socket->ssl;
        }
        return 0;
    }
    if (transport_socket->tls_state == RASTA_TLS_CONNECTION_ESTABLISHED) {
        // read as many bytes as available at this time
        do {
            receive_len = wolfSSL_read(transport_socket->ssl, received_message, (int)max_buffer_len);
            if (receive_len < 0) {
                break;
            }
            received_message += receive_len;
            max_buffer_len -= receive_len;
            received_total += receive_len;
        } while (receive_len > 0 && max_buffer_len);

        if (receive_len < 0) {
            int readErr = wolfSSL_get_error(transport_socket->ssl, 0);
            if (readErr != SSL_ERROR_WANT_READ && readErr != SSL_ERROR_WANT_WRITE) {
                fprintf(stderr, "WolfSSL decryption failed: %s.\n", wolfSSL_ERR_reason_error_string(readErr));
                abort();
            }
        }
    }
    return received_total;
}

static bool is_dtls_server(const rasta_config_tls *tls_config) {
    // client has CA cert but no server certs
    return tls_config->cert_path[0] && tls_config->key_path[0];
}

void handle_tls_mode(rasta_transport_socket *transport_socket) {
    if (is_dtls_server(transport_socket->tls_config)) {
        wolfssl_start_dtls_server(transport_socket, transport_socket->tls_config);
    } else {
        wolfssl_start_dtls_client(transport_socket, transport_socket->tls_config);
    }
}

void udp_close(rasta_transport_channel *transport_channel) {
    wolfssl_cleanup(transport_channel);
    bsd_close(transport_channel->file_descriptor);
}

size_t udp_receive(rasta_transport_socket *transport_socket, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender) {
    return wolfssl_receive_dtls(transport_socket, received_message, max_buffer_len, sender);
}

void udp_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, char *host, uint16_t port) {
    struct sockaddr_in receiver = host_port_to_sockaddr(host, port);
    udp_send_sockaddr(transport_channel, message, message_len, receiver);
}

void udp_send_sockaddr(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, struct sockaddr_in receiver) {
    wolfssl_send_dtls(transport_channel, message, message_len, &receiver);
}

bool is_dtls_conn_ready(rasta_transport_socket *socket) {
    return socket != NULL && socket->tls_state == RASTA_TLS_CONNECTION_READY;
}
