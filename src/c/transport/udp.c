
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

#include <rasta/rmemory.h>

#include "bsd_utils.h"
#include "transport.h"
#include "udp.h"

void handle_tls_mode(rasta_transport_socket *transport_socket) {
    UNUSED(transport_socket);
}

void udp_close(rasta_transport_socket *transport_socket) {
    bsd_close(transport_socket->file_descriptor);
}

size_t udp_receive(rasta_transport_socket *transport_socket, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender) {
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

void udp_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, char *host, uint16_t port) {
    bsd_send(transport_channel->file_descriptor, message, message_len, host, port);
}

void udp_send_sockaddr(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, struct sockaddr_in receiver) {
    bsd_send_sockaddr(transport_channel->file_descriptor, message, message_len, receiver);
}

bool is_dtls_conn_ready(rasta_transport_socket *socket) {
    UNUSED(socket);
    return false;
}
