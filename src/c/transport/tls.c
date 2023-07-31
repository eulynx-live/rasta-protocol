#include <arpa/inet.h>
#include <errno.h>
#include <rasta/bsd_utils.h>
#include <rasta/rmemory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

#include "tcp.h"
#include "transport.h"

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/error-crypt.h>
#include <wolfssl/wolfio.h>

#include "ssl_utils.h"

#ifdef WOLFSSL_SET_TLS13_SECRET_CB_EXISTS
/* Callback function for TLS v1.3 secrets for use with Wireshark */
static int Tls13SecretCallback(WOLFSSL *ssl, int id, const unsigned char *secret,
                               int secretSz, void *ctx) {
    int i;
    const char *str = NULL;
    unsigned char clientRandom[32];
    int clientRandomSz;
    XFILE fp = stderr;
    if (ctx) {
        fp = XFOPEN((const char *)ctx, "ab");
        if (fp == XBADFILE) {
            return BAD_FUNC_ARG;
        }
    }

    clientRandomSz = (int)wolfSSL_get_client_random(ssl, clientRandom,
                                                    sizeof(clientRandom));

    if (clientRandomSz <= 0) {
        printf("Error getting client random %d\n", clientRandomSz);
    }

#if 0
    printf("TLS Client Secret CB: Rand %d, Secret %d\n",
        clientRandomSz, secretSz);
#endif

    switch (id) {
    case CLIENT_EARLY_TRAFFIC_SECRET:
        str = "CLIENT_EARLY_TRAFFIC_SECRET";
        break;
    case EARLY_EXPORTER_SECRET:
        str = "EARLY_EXPORTER_SECRET";
        break;
    case CLIENT_HANDSHAKE_TRAFFIC_SECRET:
        str = "CLIENT_HANDSHAKE_TRAFFIC_SECRET";
        break;
    case SERVER_HANDSHAKE_TRAFFIC_SECRET:
        str = "SERVER_HANDSHAKE_TRAFFIC_SECRET";
        break;
    case CLIENT_TRAFFIC_SECRET:
        str = "CLIENT_TRAFFIC_SECRET_0";
        break;
    case SERVER_TRAFFIC_SECRET:
        str = "SERVER_TRAFFIC_SECRET_0";
        break;
    case EXPORTER_SECRET:
        str = "EXPORTER_SECRET";
        break;
    default:
        break;
    }

    if (str != NULL) {
        fprintf(fp, "%s ", str);
    }
    for (i = 0; i < clientRandomSz; i++) {
        fprintf(fp, "%02x", clientRandom[i]);
    }
    fprintf(fp, " ");
    for (i = 0; i < secretSz; i++) {
        fprintf(fp, "%02x", secret[i]);
    }
    fprintf(fp, "\n");

    if (fp != stderr) {
        XFCLOSE(fp);
    }

    return 0;
}
#endif

static void apply_tls_mode(rasta_transport_socket *transport_socket) {
    const rasta_config_tls *tls_config = transport_socket->tls_config;
    switch (tls_config->mode) {
    case TLS_MODE_DISABLED:
        transport_socket->tls_mode = TLS_MODE_DISABLED;
        break;
    case TLS_MODE_TLS_1_3:
        transport_socket->tls_mode = TLS_MODE_TLS_1_3;
        break;
    default:
        fprintf(stderr, "Unknown or unsupported TLS mode: %u", tls_config->mode);
        abort();
    }
}

static void handle_tls_mode_server(rasta_transport_socket *transport_socket) {
    apply_tls_mode(transport_socket);
    if (transport_socket->tls_mode == TLS_MODE_TLS_1_3) {
        wolfssl_start_tls_server(transport_socket, transport_socket->tls_config);
    }
}

static void handle_tls_mode_client(rasta_transport_channel *transport_channel) {
    const rasta_config_tls *tls_config = transport_channel->tls_config;
    switch (tls_config->mode) {
    case TLS_MODE_DISABLED:
        transport_channel->tls_mode = TLS_MODE_DISABLED;
        break;
    case TLS_MODE_TLS_1_3:
        transport_channel->tls_mode = TLS_MODE_TLS_1_3;
        break;
    default:
        fprintf(stderr, "Unknown or unsupported TLS mode: %u", tls_config->mode);
        abort();
    }

    if (transport_channel->tls_mode == TLS_MODE_TLS_1_3) {
        wolfssl_start_tls_client(transport_channel, transport_channel->tls_config);
    }
}

void tcp_listen(rasta_transport_socket *transport_socket) {
    if (listen(transport_socket->file_descriptor, MAX_PENDING_CONNECTIONS) < 0) {
        // listen failed
        fprintf(stderr, "error when listening to file_descriptor %d", transport_socket->file_descriptor);
        abort();
    }

    handle_tls_mode_server(transport_socket);
}

int tcp_accept(rasta_transport_socket *socket) {
    struct sockaddr_in empty_sockaddr_in;
    socklen_t sender_len = sizeof(empty_sockaddr_in);
    int socket_fd;
    if ((socket_fd = accept(socket->file_descriptor, (struct sockaddr *)&empty_sockaddr_in, &sender_len)) < 0) {
        perror("tcp failed to accept connection");
        abort();
    }

    /* Create a WOLFSSL object */
    if ((socket->ssl = wolfSSL_new(socket->ctx)) == NULL) {
        fprintf(stderr, "ERROR: failed to create WOLFSSL object\n");
        abort();
    }

    /* Attach wolfSSL to the socket */
    wolfSSL_set_fd(socket->ssl, socket_fd);
    socket->tls_state = RASTA_TLS_CONNECTION_READY;

    /* Establish TLS connection */
    int ret = wolfSSL_accept(socket->ssl);
    if (ret != WOLFSSL_SUCCESS) {
        fprintf(stderr, "wolfSSL_accept error = %d\n",
                wolfSSL_get_error(socket->ssl, ret));
        abort();
    }

    tls_pin_certificate(socket->ssl, socket->tls_config->peer_tls_cert_path);

    set_tls_async(socket_fd, socket->ssl);

    return socket_fd;
}

ssize_t tcp_receive(rasta_transport_channel *transport_channel, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender) {
    if (transport_channel->tls_mode == TLS_MODE_DISABLED) {
        ssize_t recv_len;
        struct sockaddr_in empty_sockaddr_in;
        socklen_t sender_len = sizeof(empty_sockaddr_in);

        // wait for incoming data
        if ((recv_len = recvfrom(transport_channel->file_descriptor, received_message, max_buffer_len, 0, (struct sockaddr *)sender, &sender_len)) < 0) {
            perror("an error occured while trying to receive data");
            return -1;
        }

        return (size_t)recv_len;
    } else if (transport_channel->tls_mode == TLS_MODE_TLS_1_3) {
        return wolfssl_receive_tls(transport_channel->ssl, received_message, max_buffer_len);
    }
    return 0;
}

void tcp_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len) {
    wolfssl_send_tls(transport_channel->ssl, message, message_len);
}

void tcp_close(rasta_transport_channel *transport_channel) {
    if (transport_channel->tls_mode != TLS_MODE_DISABLED) {
        wolfssl_cleanup(transport_channel);
    }

    bsd_close(transport_channel->file_descriptor);
}

int transport_connect(rasta_transport_socket *socket, rasta_transport_channel *channel, rasta_config_tls tls_config) {
    channel->file_descriptor = socket->file_descriptor;

    if (tcp_connect(channel) != 0) {
        return -1;
    }

    channel->connected = false;

    if (channel->tls_config->mode == TLS_MODE_TLS_1_3) {
        if (channel->ctx == NULL) {
            handle_tls_mode_client(channel);
        }

        channel->ssl = wolfSSL_new(channel->ctx);
        if (!channel->ssl) {
            const char *error_str = wolfSSL_ERR_reason_error_string(wolfSSL_get_error(channel->ssl, 0));
            fprintf(stderr, "Error allocating WolfSSL session: %s.\n", error_str);
            return -1;
        }

        if (tls_config.tls_hostname[0]) {
            int ret = wolfSSL_check_domain_name(channel->ssl, tls_config.tls_hostname);
            if (ret != SSL_SUCCESS) {
                fprintf(stderr, "Could not add domain name check for domain %s: %d", tls_config.tls_hostname, ret);
                return -1;
            }
        } else {
            fprintf(stderr, "No TLS hostname specified. Will accept ANY valid TLS certificate. Double-check configuration file.\n");
        }
        /* Attach wolfSSL to the socket */
        if (wolfSSL_set_fd(channel->ssl, channel->file_descriptor) != WOLFSSL_SUCCESS) {
            fprintf(stderr, "ERROR: Failed to set the file descriptor\n");
            return -1;
        }

        /* required for getting random used */
        wolfSSL_KeepArrays(channel->ssl);
#ifdef WOLFSSL_SET_TLS13_SECRET_CB_EXISTS
        /* optional logging for wireshark */
        char *sslkeylogfile_path = getenv("TLS_SECRET_LOGFILE_PATH");
        if (sslkeylogfile_path != NULL) {
            wolfSSL_set_tls13_secret_cb(channel->ssl, Tls13SecretCallback,
                                        sslkeylogfile_path);
        }
#endif

        /* Connect to wolfSSL on the server side */
        if (wolfSSL_connect(channel->ssl) != WOLFSSL_SUCCESS) {
            const char *error_str = wolfSSL_ERR_reason_error_string(wolfSSL_get_error(channel->ssl, 0));
            fprintf(stderr, "ERROR: failed to connect to wolfSSL %s.\n", error_str);
            return -1;
        }

        tls_pin_certificate(channel->ssl, tls_config.peer_tls_cert_path);

        wolfSSL_FreeArrays(channel->ssl);
        set_tls_async(channel->file_descriptor, channel->ssl);
    }

    channel->receive_event.fd = channel->file_descriptor;
    channel->receive_event_data.channel = channel;

    enable_fd_event(&channel->receive_event);

    channel->connected = true;

    return 0;
}
