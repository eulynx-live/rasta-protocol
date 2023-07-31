#include "udp.h"
#include <stdlib.h>

// this file contains implementations for the transport methods of UDP-based protocols

void transport_create_socket(struct rasta_handle *h, rasta_transport_socket *socket, int id, const rasta_config_tls *tls_config) {
    // init and bind sockets
    socket->id = id;
    udp_init(socket, tls_config);

    memset(&socket->receive_event, 0, sizeof(fd_event));
    socket->receive_event.callback = channel_receive_event;
    socket->receive_event.carry_data = &socket->receive_event_data;
    socket->receive_event.fd = socket->file_descriptor;

    memset(&socket->receive_event_data, 0, sizeof(struct receive_event_data));
    socket->receive_event_data.h = h;
    socket->receive_event_data.socket = socket;
    // Iff channel == NULL the receive event operates in 'UDP/DTLS mode'
    socket->receive_event_data.channel = NULL;
    socket->receive_event_data.connection = NULL;

    add_fd_event(h->ev_sys, &socket->receive_event, EV_READABLE);
}

bool transport_bind(rasta_transport_socket *socket, const char *ip, uint16_t port) {
    return udp_bind_device(socket, ip, port);
}

void transport_listen(rasta_transport_socket *socket) {
    enable_fd_event(&socket->receive_event);
}

int transport_accept(rasta_transport_socket *socket, struct sockaddr_in *addr) {
    UNUSED(socket);
    UNUSED(addr);
    return 0;
}

int transport_connect(rasta_transport_socket *socket, rasta_transport_channel *channel, rasta_config_tls tls_config) {
    UNUSED(tls_config);

    enable_fd_event(&socket->receive_event);

    channel->id = socket->id;
    channel->tls_mode = socket->tls_mode;
    channel->file_descriptor = socket->file_descriptor;
#ifdef ENABLE_TLS
    channel->tls_state = RASTA_TLS_CONNECTION_READY;
    channel->ctx = socket->ctx;
    channel->ssl = socket->ssl;
#endif

    // We can regard UDP/DTLS channels as 'always connected' (no re-dial possible)
    channel->connected = true;

    return 0;
}

int transport_redial(rasta_transport_channel *channel, rasta_transport_socket *socket) {
    // We can't reconnect when using UDP/DTLS
    UNUSED(channel);
    UNUSED(socket);
    return -1;
}

void transport_close(rasta_transport_channel *channel) {
    UNUSED(channel);
}

void send_callback(struct RastaByteArray data_to_send, rasta_transport_channel *channel) {
    udp_send(channel, data_to_send.bytes, data_to_send.length, channel->remote_ip_address, channel->remote_port);
}

ssize_t receive_callback(struct receive_event_data *data, unsigned char *buffer, struct sockaddr_in *sender) {
    return udp_receive(data->socket, buffer, MAX_DEFER_QUEUE_MSG_SIZE, sender);
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
