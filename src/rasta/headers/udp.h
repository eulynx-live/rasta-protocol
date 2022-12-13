/**
 * This is a module which provides a basic network communication interface for UDP
 */

#ifndef LST_SIMULATOR_UDP_H
#define LST_SIMULATOR_UDP_H

#ifdef __cplusplus
extern "C" {  // only need to export C interface if
  // used by C++ source code
#endif

#include <stdint.h>
#include <netinet/in.h>
#include "config.h"

#ifdef ENABLE_TLS
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#endif

#define IPV4_STR_LEN 16

#ifdef ENABLE_TLS
enum RastaTLSConnectionState{
        RASTA_TLS_CONNECTION_READY,
        RASTA_TLS_CONNECTION_ESTABLISHED,
        RASTA_TLS_CONNECTION_CLOSED
    };
#endif

struct RastaUDPState{
        int file_descriptor;
        enum RastaTLSMode activeMode;
        const struct RastaConfigTLS *tls_config;
#ifdef ENABLE_TLS
    WOLFSSL_CTX* ctx;
    WOLFSSL* ssl;
        enum RastaTLSConnectionState tls_state;
#endif
    };

    /**
     * This function will initialise an udp socket and return its file descriptor, which is used to reference it in later
     * function calls
     * @param state the udp socket's tls_state buffer
     * @param tls_config TLS options
     */
void udp_init(struct RastaUDPState * state,const struct RastaConfigTLS *tls_config);

    /**
     * Binds a given file descriptor to the given @p port
     * @param state tls_state with the file descriptor which will be bound to to the @p port.
     * @param port the port the socket will listen on
     */
void udp_bind(struct RastaUDPState * state, uint16_t port);

    /**
     * Binds a given file descriptor to the given @p port at the network interface with IPv4 address @p ip
     * @param state tls_state with the file descriptor which will be bound to to the @p port.
     * @param port the port the socket will listen on
     * @param ip the IPv4 address of the network interface the socket will listen on.
     */
void udp_bind_device(struct RastaUDPState * state, uint16_t port, char * ip);

    /**
     * Receive data on the given @p file descriptor and store it in the given buffer.
     * This function will block until data is received!
     * @param state tls_state which should be used to receive data
     * @param received_message a buffer where the received data will be written too. Has to be at least \p max_buffer_len long
     * @param max_buffer_len the amount of data which will be received in bytes
     * @param sender information about the sender of the data will be stored here
     * @return the amount of received bytes
     */
size_t udp_receive(struct RastaUDPState * state, unsigned char* received_message,size_t max_buffer_len, struct sockaddr_in *sender);

    /**
     * Sends a message via the given file descriptor to a @p host and @p port
     * @param state tls_state which is used to send the message
     * @param message the message which will be send
     * @param message_len the length of the @p message
     * @param host the host where the message will be send to. This has to be an IPv4 address in the format a.b.c.d
     * @param port the target port on the host
     */
void udp_send(struct RastaUDPState * state, unsigned char* message, size_t message_len, char* host, uint16_t port);

    /**
     * Sends a message via the given file descriptor to a host, the address information is stored in the
     * @p receiver struct
     * @param state tls_state which is used to send the message
     * @param message the message which will be send
     * @param message_len the length of the @p message
     * @param receiver address information about the receiver of the message
     * @param tls_config TLS configuration to use
     */
void udp_send_sockaddr(struct RastaUDPState * state, unsigned char* message, size_t message_len, struct sockaddr_in receiver);

    /**
     * Closes the udp socket
     * @param state the tls_state which identifies the socket
     */
void udp_close(struct RastaUDPState * state);

void sockaddr_to_host(struct sockaddr_in sockaddr, char* host);

#ifdef __cplusplus
}
#endif

#endif //LST_SIMULATOR_UDP_H