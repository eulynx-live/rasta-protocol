#pragma once

#include <sys/socket.h>

#ifdef ENABLE_TLS
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#endif

extern int mock_bind_call_count;

int connect(int fd, const struct sockaddr *addr, unsigned int len);
int bind(int fd, const struct sockaddr *addr, unsigned int len);

#ifdef ENABLE_TLS
int wolfSSL_connect(WOLFSSL *ssl);
#endif
