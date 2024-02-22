#include "bsd_utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //memset
#include <unistd.h>

#include "../util/rmemory.h"

struct sockaddr_in host_port_to_sockaddr(const char *host, uint16_t port) {
    struct sockaddr_in receiver;

    rmemset((char *)&receiver, 0, sizeof(receiver));
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(port);

    // convert host string to usable format
    if (inet_aton(host, &receiver.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        abort();
    }
    return receiver;
}

int bsd_create_socket(int family, int type, int protocol_type) {
    // the file descriptor of the socket
    int file_desc;

    // create a tcp socket
    if ((file_desc = socket(family, type, protocol_type)) < 0) {
        // creation failed, exit
        if (type == IPPROTO_UDP) {
            perror("The udp socket could not be initialized");
        } else if (type == IPPROTO_TCP) {
            perror("The tcp socket could not be initialized");
        } else {
            perror("The socket could not be initialized");
        }

        abort();
    }

    // Make socket reusable
    int one = 1;
    if (setsockopt(file_desc, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        abort();
    }

#ifdef SO_REUSEPORT
    if (setsockopt(file_desc, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        abort();
    }
#endif

    return file_desc;
}

bool bsd_bind_device(int file_descriptor, uint16_t port, const char *ip) {
    struct sockaddr_in local = {0};

    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(ip);

    // bind socket to port
    if (bind(file_descriptor, (struct sockaddr *)&local, sizeof(struct sockaddr_in)) < 0) {
        // bind failed
        perror("bind");
        return false;
    }

    return true;
}

void bsd_send(int file_descriptor, unsigned char *message, size_t message_len, char *host, uint16_t port) {
    struct sockaddr_in receiver = host_port_to_sockaddr(host, port);

    // send the message using the other send function
    bsd_send_sockaddr(file_descriptor, message, message_len, receiver);
}

void bsd_send_sockaddr(int file_descriptor, unsigned char *message, size_t message_len, struct sockaddr_in receiver) {
    if (sendto(file_descriptor, message, message_len, 0, (struct sockaddr *)&receiver, sizeof(receiver)) < 0) {
        perror("failed to send data");
        abort();
    }
}

void bsd_close(int file_descriptor) {
    if (file_descriptor >= 0) {
        if (close(file_descriptor) < 0) {
            perror("close");
            abort();
        }
    }
}

int getSO_ERROR(int fd) {
    int err = 1;
    socklen_t len = sizeof err;
    if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
        abort();
    if (err)
        errno = err; // set errno to the socket SO_ERROR
    return err;
}

void sockaddr_to_host(struct sockaddr_in sockaddr, char *host) {
    inet_ntop(AF_INET, &(sockaddr.sin_addr), host, IPV4_STR_LEN);
}
