#include <arpa/inet.h>
#include <errno.h>
#include <rasta/bsd_utils.h>
#include <rasta/rmemory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

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

void tcp_init(rasta_transport_socket *transport_state, const rasta_config_tls *tls_config) {
    transport_state->tls_config = tls_config;
    transport_state->file_descriptor = bsd_create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

static void apply_tls_mode(rasta_transport_socket *transport_state) {
    const rasta_config_tls *tls_config = transport_state->tls_config;
    switch (tls_config->mode) {
    case TLS_MODE_DISABLED:
        transport_state->tls_mode = TLS_MODE_DISABLED;
        break;
    case TLS_MODE_TLS_1_3:
        transport_state->tls_mode = TLS_MODE_TLS_1_3;
        break;
    default:
        fprintf(stderr, "Unknown or unsupported TLS mode: %u", tls_config->mode);
        abort();
    }
}

static void handle_tls_mode_server(rasta_transport_socket *transport_state) {
    apply_tls_mode(transport_state);
    if (transport_state->tls_mode == TLS_MODE_TLS_1_3) {
        wolfssl_start_tls_server(transport_state, transport_state->tls_config);
    }
}

static void handle_tls_mode_client(rasta_transport_channel *transport_state) {
    const rasta_config_tls *tls_config = transport_state->tls_config;
    switch (tls_config->mode) {
    case TLS_MODE_DISABLED:
        transport_state->tls_mode = TLS_MODE_DISABLED;
        break;
    case TLS_MODE_TLS_1_3:
        transport_state->tls_mode = TLS_MODE_TLS_1_3;
        break;
    default:
        fprintf(stderr, "Unknown or unsupported TLS mode: %u", tls_config->mode);
        abort();
    }

    if (transport_state->tls_mode == TLS_MODE_TLS_1_3) {
        wolfssl_start_tls_client(transport_state, transport_state->tls_config);
    }
}

void tcp_bind(rasta_transport_socket *transport_state, uint16_t port) {
    bsd_bind_port(transport_state->file_descriptor, port);
}

void tcp_bind_device(rasta_transport_socket *transport_state, const char *ip, uint16_t port) {
    bsd_bind_device(transport_state->file_descriptor, port, ip);
}

void tcp_listen(rasta_transport_socket *transport_state) {
    if (listen(transport_state->file_descriptor, MAX_PENDING_CONNECTIONS) < 0) {
        // listen failed
        fprintf(stderr, "error when listening to file_descriptor %d", transport_state->file_descriptor);
        abort();
    }

    handle_tls_mode_server(transport_state);
}

int do_accept(rasta_transport_socket *socket) {
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

int tcp_connect(rasta_transport_channel *channel) {
    struct sockaddr_in server;

    rmemset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(channel->remote_port);

    // convert host string to usable format
    if (inet_aton(channel->remote_ip_address, &server.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        abort();
    }

    if (connect(channel->file_descriptor, (struct sockaddr *)&server, sizeof(server)) < 0) {
        channel->connected = false;
        return 1;
    }

    channel->connected = true;
    return 0;
}

ssize_t do_receive(rasta_transport_channel *transport_state, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender) {
    if (transport_state->tls_mode == TLS_MODE_DISABLED) {
        ssize_t recv_len;
        struct sockaddr_in empty_sockaddr_in;
        socklen_t sender_len = sizeof(empty_sockaddr_in);

        // wait for incoming data
        if ((recv_len = recvfrom(transport_state->file_descriptor, received_message, max_buffer_len, 0, (struct sockaddr *)sender, &sender_len)) < 0) {
            perror("an error occured while trying to receive data");
            return -1;
        }

        return (size_t)recv_len;
    } else if (transport_state->tls_mode == TLS_MODE_TLS_1_3) {
        return wolfssl_receive_tls(transport_state->ssl, received_message, max_buffer_len);
    }
    return 0;
}

void do_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len) {
    wolfssl_send_tls(transport_channel->ssl, message, message_len);
}

void tcp_close(rasta_transport_socket *transport_state) {
    if (transport_state->tls_mode != TLS_MODE_DISABLED) {
        wolfssl_cleanup(transport_state);
    }

    bsd_close(transport_state->file_descriptor);
}

void transport_listen(struct rasta_handle *h, rasta_transport_socket *socket) {
    UNUSED(h);
    tcp_listen(socket);

    // Register accept event

    enable_fd_event(&socket->accept_event);
}

int transport_accept(rasta_transport_socket *socket, struct sockaddr_in *addr) {
    int fd = do_accept(socket);
    // channel->id = socket->id;
    // channel->remote_port = 0;
    // channel->remote_ip_address[0] = '\0';
    // channel->send_callback = send_callback;
    // channel->tls_mode = socket->tls_mode;
    // channel->file_descriptor = fd;
    // channel->connected = true;
    // channel->ssl = socket->ssl;
    // channel->ctx = socket->ctx;

    // struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    if (getpeername(fd, (struct sockaddr *)addr, &addr_size) != 0) {
        perror("tcp failed to resolve peer name");
        abort();
    }

    // char str[INET_ADDRSTRLEN];
    // inet_ntop(AF_INET, &addr.sin_addr, str, INET_ADDRSTRLEN);
    // memcpy(channel->remote_ip_address, str, INET_ADDRSTRLEN);
    // channel->remote_port = ntohs(addr.sin_port);

    return fd;
}

int transport_connect(struct rasta_connection *h, rasta_transport_socket *socket, rasta_transport_channel *channel) {
    channel->file_descriptor = socket->file_descriptor;

    if (tcp_connect(channel) != 0) {
        return -1;
    }

    channel->connected = false;

    if (channel->ctx == NULL) {
        handle_tls_mode_client(channel);
    }

    channel->ssl = wolfSSL_new(channel->ctx);
    if (!channel->ssl) {
        const char *error_str = wolfSSL_ERR_reason_error_string(wolfSSL_get_error(channel->ssl, 0));
        fprintf(stderr, "Error allocating WolfSSL session: %s.\n", error_str);
        return -1;
    }

    if (h->config->tls.tls_hostname[0]) {
        int ret = wolfSSL_check_domain_name(channel->ssl, h->config->tls.tls_hostname);
        if (ret != SSL_SUCCESS) {
            fprintf(stderr, "Could not add domain name check for domain %s: %d", h->config->tls.tls_hostname, ret);
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
    char* sslkeylogfile_path = getenv("TLS_SECRET_LOGFILE_PATH");
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

    tls_pin_certificate(channel->ssl, h->config->tls.peer_tls_cert_path);

    wolfSSL_FreeArrays(channel->ssl);
    set_tls_async(channel->file_descriptor, channel->ssl);

    channel->receive_event.fd = channel->file_descriptor;
    channel->receive_event_data.channel = channel;

    enable_fd_event(&channel->receive_event);

    channel->connected = true;

    return 0;
}

int transport_redial(rasta_transport_channel* channel) {
    // TODO: just reconnecting does not work!
    if (tcp_connect(channel) != 0) {
        return -1;
    }

    enable_fd_event(&channel->receive_event);
    return 0;
}

void transport_close(rasta_transport_channel *channel) {
    if (channel->connected) {
        bsd_close(channel->file_descriptor);
        if (channel->ssl) {
            wolfSSL_shutdown(channel->ssl);
            wolfSSL_free(channel->ssl);
        }
    }

    disable_fd_event(&channel->receive_event);
    channel->connected = false;
}

void send_callback(redundancy_mux *mux, struct RastaByteArray data_to_send, rasta_transport_channel *channel, unsigned int channel_index) {
    UNUSED(mux);
    UNUSED(channel_index);
    do_send(channel, data_to_send.bytes, data_to_send.length);
}

ssize_t receive_callback(struct receive_event_data *data, unsigned char *buffer, struct sockaddr_in *sender) {
    // TODO: exchange MAX_DEFER_QUEUE_MSG_SIZE by something depending on send_max (i.e. the receive buffer size)
    // search for connected_recv_buffer_size
    // TODO: Manage possible remaining data in the receive buffer on next call to rasta_recv
    return do_receive(data->channel, buffer, MAX_DEFER_QUEUE_MSG_SIZE, sender);
}

void transport_create_socket(struct rasta_handle *h, rasta_transport_socket *socket, int id, const rasta_config_tls *tls_config) {
    // init socket
    socket->id = id;
    tcp_init(socket, tls_config);

    memset(&socket->accept_event, 0, sizeof(fd_event));

    socket->accept_event.callback = channel_accept_event;
    socket->accept_event.carry_data = &socket->accept_event_data;
    socket->accept_event.fd = socket->file_descriptor;

    socket->accept_event_data.event = &socket->accept_event;
    socket->accept_event_data.socket = socket;
    socket->accept_event_data.h = h;

    add_fd_event(h->ev_sys, &socket->accept_event, EV_READABLE);
}

void transport_bind(struct rasta_handle *h, rasta_transport_socket *socket, const char *ip, uint16_t port) {
    UNUSED(h);
    tcp_bind_device(socket, ip, port);
}

void transport_init(struct rasta_handle *h, rasta_transport_channel* channel, unsigned id, const char *host, uint16_t port, const rasta_config_tls *tls_config) {
    transport_init_base(h, channel, id, host, port, tls_config);
}
