#pragma once

#include <sys/socket.h>

#ifdef ENABLE_TLS
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#endif

int connect(int fd, const struct sockaddr *addr, unsigned int len);

#ifdef ENABLE_TLS
int wolfSSL_connect(WOLFSSL *ssl);
#endif
