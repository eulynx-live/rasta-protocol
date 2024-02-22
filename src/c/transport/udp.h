/**
 * This is a module which provides a basic network communication interface for UDP
 */
#pragma once

#include <netinet/in.h>
#include <stdint.h>

#include "transport.h"

void handle_tls_mode(rasta_transport_socket *transport_socket);

/**
 * Receive data on the given @p file descriptor and store it in the given buffer.
 * This function will block until data is received!
 * @param transport_socket transport_socket which should be used to receive data
 * @param received_message a buffer where the received data will be written too. Has to be at least \p max_buffer_len long
 * @param max_buffer_len the amount of data which will be received in bytes
 * @param sender information about the sender of the data will be stored here
 * @return the amount of received bytes
 */
size_t udp_receive(rasta_transport_socket *transport_socket, unsigned char *received_message, size_t max_buffer_len, struct sockaddr_in *sender);

/**
 * Sends a message via the given file descriptor to a @p host and @p port
 * @param transport_channel transport_channel which is used to send the message
 * @param message the message which will be send
 * @param message_len the length of the @p message
 * @param host the host where the message will be send to. This has to be an IPv4 address in the format a.b.c.d
 * @param port the target port on the host
 */
void udp_send(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, char *host, uint16_t port);

/**
 * Sends a message via the given file descriptor to a host, the address information is stored in the
 * @p receiver struct
 * @param transport_channel transport_channel which is used to send the message
 * @param message the message which will be send
 * @param message_len the length of the @p message
 * @param receiver address information about the receiver of the message
 * @param tls_config TLS configuration to use
 */
void udp_send_sockaddr(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, struct sockaddr_in receiver);

/**
 * Closes the udp socket
 * @param transport_socket the rasta_transport_socket which identifies the socket
 */
void udp_close(rasta_transport_socket *transport_socket);
