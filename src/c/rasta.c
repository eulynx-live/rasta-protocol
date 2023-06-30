#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <rasta/event_system.h>
#include <rasta/rasta.h>
#include <rasta/rasta_init.h>
#include <rasta/rastahandle.h>
#include <rasta/rmemory.h>

#include "experimental/handlers.h"
#include "retransmission/handlers.h"
#include "retransmission/safety_retransmission.h"

void log_main_loop_state(struct rasta_handle *h, event_system *ev_sys, const char *message) {
    int fd_event_count = 0, fd_event_active_count = 0, timed_event_count = 0, timed_event_active_count = 0;
    for (fd_event *ev = ev_sys->fd_events.first; ev; ev = ev->next) {
        fd_event_count++;
        fd_event_active_count += !!ev->enabled;
    }
    for (timed_event *ev = ev_sys->timed_events.first; ev; ev = ev->next) {
        timed_event_count++;
        timed_event_active_count += !!ev->enabled;
    }
    logger_log(h->logger, LOG_LEVEL_DEBUG, "RaSTA EVENT-SYSTEM", "%s | %d/%d fd events and %d/%d timed events active",
               message, fd_event_active_count, fd_event_count, timed_event_active_count, timed_event_count);
}

void rasta_bind(rasta_lib_configuration_t user_configuration) {
    redundancy_mux_bind(&user_configuration->h);
}

void rasta_listen(rasta_lib_configuration_t user_configuration) {
    sr_listen(&user_configuration->h);
}

struct rasta_connection * rasta_accept(rasta_lib_configuration_t user_configuration) {
    struct rasta_handle *h = &user_configuration->h;
    event_system *event_system = &user_configuration->rasta_lib_event_system;

    // accept events were already prepared by rasta_listen
    // event system will break when we have received the first heartbeat of a new connection
    log_main_loop_state(h, event_system, "event-system started");
    event_system_start(event_system);

    for (unsigned i = 0; i < h->rasta_connections_length; i++) {
        if (h->rasta_connections[i].is_new) {
            h->rasta_connections[i].is_new = false;
            return &h->rasta_connections[i];
        }
    }

    return NULL;
}

struct rasta_connection* rasta_connect(rasta_lib_configuration_t user_configuration, unsigned long id) {
    return sr_connect(&user_configuration->h, id);
}

int rasta_recv(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len) {
    struct rasta_handle *h = &user_configuration->h;
    event_system *event_system = &user_configuration->rasta_lib_event_system;

    sr_set_receive_buffer(buf, len);

    size_t received_data_len = 0;

    while (connection->current_state == RASTA_CONNECTION_UP && received_data_len == 0) {
        log_main_loop_state(h, event_system, "event-system started");
        event_system_start(event_system);

        received_data_len = sr_get_received_data_len();
    }

    if (connection->current_state != RASTA_CONNECTION_UP) {
        // TODO: If sockets are broken, their event handlers have to be removed...
        return -1;
    }

    return sr_get_received_data_len();
}

int rasta_send(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len) {
    struct RastaMessageData messageData1;
    allocateRastaMessageData(&messageData1, 1);
    messageData1.data_array[0].bytes = buf;
    messageData1.data_array[0].length = len;

    sr_send(&user_configuration->h, connection, messageData1);
    rfree(messageData1.data_array);
    return 0;
}

void rasta_disconnect(struct rasta_connection *connection) {
    sr_disconnect(connection);
}

void rasta_cleanup(rasta_lib_configuration_t user_configuration) {
    sr_cleanup(&user_configuration->h);
    for (unsigned i = 0; i < user_configuration->h.rasta_connections_length; i++) {
        fifo_destroy(&user_configuration->h.rasta_connections[i].fifo_retransmission);
        fifo_destroy(&user_configuration->h.rasta_connections[i].fifo_send);
    }
    rfree(user_configuration->h.rasta_connections);

}