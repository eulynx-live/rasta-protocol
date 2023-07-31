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

void tcp_init(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config) {
    transport_socket->tls_config = tls_config;
    transport_socket->file_descriptor = bsd_create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

bool tcp_bind_device(rasta_transport_socket *transport_socket, const char *ip, uint16_t port) {
    return bsd_bind_device(transport_socket->file_descriptor, port, ip);
}

void tcp_listen(rasta_transport_socket *transport_socket) {
    if (listen(transport_socket->file_descriptor, MAX_PENDING_CONNECTIONS) < 0) {
        // listen failed
        fprintf(stderr, "error when listening to file_descriptor %d", transport_socket->file_descriptor);
        abort();
    }
}

int tcp_accept(rasta_transport_socket *transport_socket) {
    struct sockaddr_in empty_sockaddr_in;
    socklen_t sender_len = sizeof(empty_sockaddr_in);
    int socket;
    if ((socket = accept(transport_socket->file_descriptor, (struct sockaddr *)&empty_sockaddr_in, &sender_len)) < 0) {
        perror("tcp failed to accept connection");
        abort();
    }

    return socket;
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
    }
    return 0;
}

void tcp_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len) {
    if (send(transport_channel->file_descriptor, message, message_len, 0) < 0) {
        perror("failed to send data");
        abort();
    }
}

void tcp_close(rasta_transport_channel *transport_channel) {
    bsd_close(transport_channel->file_descriptor);
}

int transport_connect(rasta_transport_socket *socket, rasta_transport_channel *channel, rasta_config_tls tls_config) {
    UNUSED(tls_config);
    channel->file_descriptor = socket->file_descriptor;

    if (tcp_connect(channel) != 0) {
        return -1;
    };

    channel->receive_event.fd = channel->file_descriptor;
    channel->receive_event_data.channel = channel;

    enable_fd_event(&channel->receive_event);

    return 0;
}
