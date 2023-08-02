/**
 * This is a module which provides a basic network communication interface for UDP
 */
#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <netinet/in.h>
#include <stdint.h>

#include "transport.h"

void handle_tls_mode(rasta_transport_socket *transport_socket);

/**
 * This function will initialise an udp socket and return its file descriptor, which is used to reference it in later
 * function calls
 * @param transport_socket the udp transport_socket buffer
 * @param tls_config TLS options
 */
void udp_init(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config);

/**
 * Binds a given file descriptor to the given @p port at the network interface with IPv4 address @p ip
 * @param transport_socket transport_socket with the file descriptor which will be bound to to the @p port.
 * @param port the port the socket will listen on
 * @param ip the IPv4 address of the network interface the socket will listen on.
 */
bool udp_bind_device(rasta_transport_socket *transport_socket, const char *ip, uint16_t port);

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
 * Closes the udp channel
 * @param transport_channel the rasta_transport_channel which identifies the channel
 */
// TODO: This is unused! Why? Why does udp_close even exist?
// TODO: transport_channel signature needed to compile, but doesn't make sense here!
void udp_close(rasta_transport_channel *transport_channel);

#ifdef __cplusplus
}
#endif
