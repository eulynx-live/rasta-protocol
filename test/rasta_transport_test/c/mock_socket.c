#include "mock_socket.h"

int connect(int fd, const struct sockaddr *addr, unsigned int len) {
    return 0;
}

int mock_bind_call_count;

int bind(int fd, const struct sockaddr *addr, unsigned int len) {
    mock_bind_call_count ++;
    return 0;
}

#ifdef ENABLE_TLS
int wolfSSL_connect(WOLFSSL *ssl) {
    return WOLFSSL_SUCCESS;
}
#endif
