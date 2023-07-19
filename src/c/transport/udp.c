
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

#include <rasta/bsd_utils.h>
#include <rasta/rmemory.h>
#include "udp.h"

#include "transport.h"

#define MAX_WARNING_LENGTH_BYTES 128

static void handle_tls_mode(rasta_transport_socket *transport_socket) {
    const rasta_config_tls *tls_config = transport_socket->tls_config;
    switch (tls_config->mode) {
        case TLS_MODE_DISABLED: {
            transport_socket->tls_mode = TLS_MODE_DISABLED;
            break;
        }
        default: {
            fprintf(stderr, "Unknown or unsupported TLS mode: %u", tls_config->mode);
            abort();
        }
    }
}

bool udp_bind(rasta_transport_socket *transport_socket, uint16_t port) {
    struct sockaddr_in local;
    rmemset((char *)&local, 0, sizeof(local));

    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    if (bind(transport_socket->file_descriptor, (struct sockaddr *)&local, sizeof(local)) == -1) {
        // bind failed
        perror("bind");
        return false;
    }
    handle_tls_mode(transport_socket);
    return true;
}

bool udp_bind_device(rasta_transport_socket *transport_socket, const char *ip, uint16_t port) {
    struct sockaddr_in local = {0};

    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(ip);

    // bind socket to port
    if (bind(transport_socket->file_descriptor, (struct sockaddr *)&local, sizeof(struct sockaddr_in)) == -1) {
        // bind failed
        perror("bind");
        return false;
    }

    handle_tls_mode(transport_socket);
    return true;
}

void udp_close(rasta_transport_socket *transport_socket) {
    int file_descriptor = transport_socket->file_descriptor;
    if (file_descriptor >= 0) {
        getSO_ERROR(file_descriptor);                   // first clear any errors, which can cause close to fail
        if (shutdown(file_descriptor, SHUT_RDWR) < 0)   // secondly, terminate the 'reliable' delivery
            if (errno != ENOTCONN && errno != EINVAL) { // SGI causes EINVAL
                perror("shutdown");
                abort();
            }
        if (close(file_descriptor) < 0) // finally call close()
        {
            perror("close");
            abort();
        }
    }
}

size_t udp_receive(rasta_transport_socket *transport_socket, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender) {
    if (transport_socket->tls_mode == TLS_MODE_DISABLED) {
        ssize_t recv_len;
        struct sockaddr_in empty_sockaddr_in;
        socklen_t sender_len = sizeof(empty_sockaddr_in);

        // wait for incoming data
        if ((recv_len = recvfrom(transport_socket->file_descriptor, received_message, max_buffer_len, 0, (struct sockaddr *)sender, &sender_len)) == -1) {
            perror("an error occured while trying to receive data");
            abort();
        }

        return (size_t)recv_len;
    }
    return 0;
}

void udp_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, char *host, uint16_t port) {
    struct sockaddr_in receiver = host_port_to_sockaddr(host, port);
    if (transport_channel->tls_mode == TLS_MODE_DISABLED) {
        // send the message using the other send function
        udp_send_sockaddr(transport_channel, message, message_len, receiver);
    }
}

void udp_send_sockaddr(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, struct sockaddr_in receiver) {
    if (transport_channel->tls_mode == TLS_MODE_DISABLED) {
        if (sendto(transport_channel->file_descriptor, message, message_len, 0, (struct sockaddr *)&receiver, sizeof(receiver)) ==
            -1) {
            perror("failed to send data");
            abort();
        }
    }
}

void udp_init(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config) {
    // the file descriptor of the socket
    int file_desc;

    transport_socket->tls_config = tls_config;

    // create a udp socket
    if ((file_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        // creation failed, exit
        perror("The udp socket could not be initialized");
        abort();
    }
    transport_socket->file_descriptor = file_desc;
}

void transport_create_socket(struct rasta_handle *h, rasta_transport_socket *socket, int id, const rasta_config_tls *tls_config) {
    // init and bind sockets
    socket->id = id;
    udp_init(socket, tls_config);

    memset(&socket->receive_event, 0, sizeof(fd_event));

    socket->receive_event.callback = channel_receive_event;
    socket->receive_event.carry_data = &socket->receive_event_data;
    socket->receive_event.fd = socket->file_descriptor;

    socket->receive_event_data.h = h;
    socket->receive_event_data.connection = NULL;
    socket->receive_event_data.socket = socket;
    // Iff channel == NULL the receive event operates in 'UDP/DTLS mode'
    socket->receive_event_data.channel = NULL;

    add_fd_event(h->ev_sys, &socket->receive_event, EV_READABLE);
}

int transport_connect(rasta_transport_socket *socket, rasta_transport_channel *channel, rasta_config_tls tls_config) {
    UNUSED(tls_config);

    enable_fd_event(&socket->receive_event);

    channel->id = socket->id;
    channel->tls_mode = socket->tls_mode;
    channel->file_descriptor = socket->file_descriptor;

    // We can regard UDP channels as 'always connected' (no re-dial possible)
    channel->connected = true;

    return 0;
}

int transport_redial(rasta_transport_channel *channel, rasta_transport_socket *socket) {
    // We can't reconnect when using UDP
    UNUSED(channel); UNUSED(socket);
    return -1;
}

void transport_close(rasta_transport_channel *channel) {
    UNUSED(channel);
}

void send_callback(redundancy_mux *mux, struct RastaByteArray data_to_send, rasta_transport_channel *channel, unsigned int channel_index) {
    UNUSED(mux); UNUSED(channel_index);
    udp_send(channel, data_to_send.bytes, data_to_send.length, channel->remote_ip_address, channel->remote_port);
}

ssize_t receive_callback(struct receive_event_data *data, unsigned char *buffer, struct sockaddr_in *sender) {
    return udp_receive(data->socket, buffer, MAX_DEFER_QUEUE_MSG_SIZE, sender);
}

void transport_listen(struct rasta_handle *h, rasta_transport_socket *socket) {
    UNUSED(h);
    enable_fd_event(&socket->receive_event);
}

bool transport_bind(struct rasta_handle *h, rasta_transport_socket *socket, const char *ip, uint16_t port) {
    UNUSED(h);
    return udp_bind_device(socket, ip, port);
}

int transport_accept(rasta_transport_socket *socket, struct sockaddr_in *addr) {
    UNUSED(socket); UNUSED(addr);
    return 0;
}

void transport_init(struct rasta_handle *h, rasta_transport_channel* channel, unsigned id, const char *host, uint16_t port, const rasta_config_tls *tls_config) {
    transport_init_base(h, channel, id, host, port, tls_config);
}
