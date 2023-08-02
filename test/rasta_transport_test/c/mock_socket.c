#include "mock_socket.h"

int connect(int fd, const struct sockaddr *addr, unsigned int len) {
    return 0;
}

#ifdef ENABLE_TLS
int wolfSSL_connect(WOLFSSL *ssl) {
    return WOLFSSL_SUCCESS;
}
#endif
