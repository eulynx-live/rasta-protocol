#pragma once

#include <sys/socket.h>

int connect(int fd, const struct sockaddr *addr, unsigned int len);
