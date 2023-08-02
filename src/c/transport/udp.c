
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

#include "udp.h"
#include <rasta/bsd_utils.h>
#include <rasta/rmemory.h>

#include "transport.h"

void handle_tls_mode(rasta_transport_socket *transport_socket) {
    const rasta_config_tls *tls_config = transport_socket->tls_config;
    if (tls_config->mode != TLS_MODE_DISABLED) {
        fprintf(stderr, "Unknown or unsupported TLS mode: %u", tls_config->mode);
        abort();
    }
}

void udp_close(rasta_transport_channel *transport_channel) {
    int file_descriptor = transport_channel->file_descriptor;
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
    if (transport_socket->tls_config->mode == TLS_MODE_DISABLED) {
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
    if (transport_channel->tls_config->mode == TLS_MODE_DISABLED) {
        // send the message using the other send function
        udp_send_sockaddr(transport_channel, message, message_len, receiver);
    }
}

void udp_send_sockaddr(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, struct sockaddr_in receiver) {
    if (transport_channel->tls_config->mode == TLS_MODE_DISABLED) {
        if (sendto(transport_channel->file_descriptor, message, message_len, 0, (struct sockaddr *)&receiver, sizeof(receiver)) ==
            -1) {
            perror("failed to send data");
            abort();
        }
    }
}
