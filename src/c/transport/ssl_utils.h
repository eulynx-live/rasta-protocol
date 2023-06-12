#pragma once

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#include "transport.h"

typedef void(WOLFSSL_ASYNC_METHOD)(WOLFSSL *, int);
typedef int(WOLFSSL_SET_PEER_METHOD)(WOLFSSL *, void *, unsigned int);

void wolfssl_initialize_if_necessary();

void wolfssl_start_dtls_server(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config);

void wolfssl_start_tls_server(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config);

void wolfssl_start_server(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config, WOLFSSL_METHOD *server_method);

void set_dtls_async(rasta_transport_socket *transport_socket);

void set_tls_async(int fd, WOLFSSL *ssl);

void set_socket_async(rasta_transport_channel *transport_channel, WOLFSSL_ASYNC_METHOD *wolfssl_async_method);

void wolfssl_start_dtls_client(rasta_transport_socket *transport_socket, const rasta_config_tls *tls_config);

void wolfssl_start_tls_client(rasta_transport_channel *transport_channel, const rasta_config_tls *tls_config);

void wolfssl_start_client(rasta_transport_channel *transport_channel, const rasta_config_tls *tls_config, WOLFSSL_METHOD *client_method);

void wolfssl_send(WOLFSSL *ssl, unsigned char *message, size_t message_len);

void wolfssl_send_tls(WOLFSSL *ssl, unsigned char *message, size_t message_len);

void wolfssl_send_dtls(rasta_transport_channel *transport_channel, unsigned char *message, size_t message_len, struct sockaddr_in *receiver);

ssize_t wolfssl_receive_tls(WOLFSSL *ssl, unsigned char *received_message, size_t max_buffer_len);

void wolfssl_cleanup(rasta_transport_socket *transport_socket);

void tls_pin_certificate(WOLFSSL *ssl, const char *peer_tls_cert_path);
