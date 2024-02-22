#pragma once

#include "../util/event_system.h"

typedef struct rasta_transport_socket rasta_transport_socket;
typedef struct rasta_transport_channel rasta_transport_channel;
typedef struct rasta_redundancy_channel rasta_redundancy_channel;
typedef struct rasta_connection rasta_connection;

struct accept_event_data {
    fd_event *event;
    rasta_transport_socket *socket;
    struct rasta_handle *h;
};

struct receive_event_data {
    struct rasta_handle *h;
    rasta_connection *connection;
    rasta_transport_socket *socket;
    rasta_transport_channel *channel;
};

int channel_accept_event(void *carry_data, int fd);
int channel_receive_event(void *carry_data, int fd);

int data_send_event(void *carry_data, int fd);
int heartbeat_send_event(void *carry_data, int fd);
int event_connection_expired(void *carry_data, int fd);

int send_timed_key_exchange(void *arg, int fd);
