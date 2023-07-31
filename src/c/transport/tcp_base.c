#include "tcp.h"
#include <rasta/bsd_utils.h>
#include <stdlib.h>

void transport_create_socket(struct rasta_handle *h, rasta_transport_socket *socket, int id, const rasta_config_tls *tls_config) {
    // init socket
    socket->id = id;
    tcp_init(socket, tls_config);

    // register accept event
    memset(&socket->accept_event, 0, sizeof(fd_event));

    socket->accept_event.callback = channel_accept_event;
    socket->accept_event.carry_data = &socket->accept_event_data;
    socket->accept_event.fd = socket->file_descriptor;

    socket->accept_event_data.event = &socket->accept_event;
    socket->accept_event_data.socket = socket;
    socket->accept_event_data.h = h;

    add_fd_event(h->ev_sys, &socket->accept_event, EV_READABLE);
}

bool transport_bind(rasta_transport_socket *socket, const char *ip, uint16_t port) {
    return tcp_bind_device(socket, ip, port);
}

void transport_listen(rasta_transport_socket *socket) {
    tcp_listen(socket);
    enable_fd_event(&socket->accept_event);
}

int transport_accept(rasta_transport_socket *socket, struct sockaddr_in *addr) {
    int fd = tcp_accept(socket);

    socklen_t addr_size = sizeof(struct sockaddr_in);
    if (getpeername(fd, (struct sockaddr *)addr, &addr_size) != 0) {
        perror("tcp failed to resolve peer name");
        abort();
    }

    return fd;
}

int transport_redial(rasta_transport_channel *channel, rasta_transport_socket *socket) {
    // create a new socket (closed socket cannot be reused)
    socket->file_descriptor = bsd_create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // bind new socket to the configured ip/port
    rasta_handle *h = socket->accept_event_data.h;
    const rasta_ip_data *ip_data = &h->mux.config->redundancy.connections.data[socket->id];
    transport_bind(socket, ip_data->ip, (uint16_t)ip_data->port);

    if (transport_connect(socket, channel, *channel->tls_config) != 0) {
        return -1;
    }

    socket->receive_event.fd = socket->file_descriptor;
    socket->accept_event.fd = socket->file_descriptor;

    return 0;
}

void transport_close(rasta_transport_channel *channel) {
    if (channel->connected) {
        bsd_close(channel->file_descriptor);
        channel->file_descriptor = -1;
        channel->connected = false;
#ifdef ENABLE_TLS
        if (channel->ssl) {
            wolfSSL_shutdown(channel->ssl);
            wolfSSL_free(channel->ssl);
        }
#endif
    }

    disable_fd_event(&channel->receive_event);
}

void send_callback(struct RastaByteArray data_to_send, rasta_transport_channel *channel) {
    tcp_send(channel, data_to_send.bytes, data_to_send.length);
}

ssize_t receive_callback(struct receive_event_data *data, unsigned char *buffer, struct sockaddr_in *sender) {
    // TODO: exchange MAX_DEFER_QUEUE_MSG_SIZE by something depending on send_max (i.e. the receive buffer size)
    // search for connected_recv_buffer_size
    // TODO: Manage possible remaining data in the receive buffer on next call to rasta_recv
    return tcp_receive(data->channel, buffer, MAX_DEFER_QUEUE_MSG_SIZE, sender);
}
