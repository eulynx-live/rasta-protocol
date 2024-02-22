#include "rastahandle.h"

#include <stdlib.h>

#include "util/rmemory.h"

struct rasta_notification_result sr_create_notification_result(struct rasta_handle *handle, struct rasta_connection *connection) {
    struct rasta_notification_result r = {NULL};
    UNUSED(handle);
    UNUSED(connection);

    return r;
}

/**
 * the is the function that handles the call of the onConnectionStateChange notification pointer.
 * this runs on a separate thread
 * @param connection the connection that will be used
 * @return unused
 */
void on_constatechange_call(struct rasta_notification_result *result) {
    UNUSED(result);
    // (*result->handle->notifications.on_connection_state_change)(result);
}

/**
 * fires the onConnectionStateChange event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param connection the connection that is used
 */
void fire_on_connection_state_change(struct rasta_notification_result result) {
    UNUSED(result);
    // if (result.handle->notifications.on_connection_state_change == NULL) {
    //     // notification not set, do nothing
    //     return;
    // }

    // on_constatechange_call(&result);
}

void on_receive_call(struct rasta_notification_result *result) {
    UNUSED(result);
    // (*result->handle->notifications.on_receive)(result);
}

/**
 * fires the onConnectionStateChange event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param connection the connection that is used
 */
void fire_on_receive(struct rasta_notification_result result) {
    UNUSED(result);
    // if (result.handle->notifications.on_receive == NULL) {
    //     // notification not set, do nothing
    //     return;
    // }

    // // create container
    // struct rasta_notification_result *container = rmalloc(sizeof(struct rasta_notification_result));
    // *container = result;

    // on_receive_call(&result);
}

void on_discrequest_change_call(struct rasta_disconnect_notification_result *container) {
    UNUSED(container);
    // struct rasta_disconnect_notification_result *result = (struct rasta_disconnect_notification_result *)container;

    // (*result->result.handle->notifications.on_disconnection_request_received)(&result->result, result->reason, result->detail);

    // // free container
    // rfree(container);
}

/**
 * fires the onConnectionStateChange event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param connection the connection that is used
 */
void fire_on_discrequest_state_change(struct rasta_notification_result result, struct RastaDisconnectionData data) {
    UNUSED(result);
    UNUSED(data);

    // if (result.handle->notifications.on_disconnection_request_received == NULL) {
    //     // notification not set, do nothing
    //     return;
    // }

    // // create container

    // struct rasta_disconnect_notification_result *container = rmalloc(sizeof(struct rasta_disconnect_notification_result));
    // container->result = result;
    // container->reason = data.reason;
    // container->detail = data.details;

    // on_discrequest_change_call(container);
}

/**
 * the is the function that handles the call of the onDiagnosticNotification notification pointer.
 * this runs on a separate thread
 * @param connection the connection that will be used
 * @return unused
 */
void on_diagnostic_call(void *container) {
    UNUSED(container);
    // struct rasta_notification_result *result = (struct rasta_notification_result *)container;

    // (*result->handle->notifications.on_diagnostic_notification)(result);

    // // free container
    // rfree(container);
}

/**
 * fires the onDiagnosticNotification event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param connection the connection that is used
 */
void fire_on_diagnostic_notification(struct rasta_notification_result result) {
    UNUSED(result);

    // if (result.handle->notifications.on_diagnostic_notification == NULL) {
    //     // notification not set, do nothing
    //     return;
    // }

    // if (result.connection.received_diagnostic_message_count <= 0) {
    //     // no diagnostic notification to send
    //     return;
    // }

    // // create container
    // on_diagnostic_call(&result);
}

void on_handshake_complete_call(struct rasta_notification_result *result) {
    UNUSED(result);
    // (*result->handle->notifications.on_handshake_complete)(result);
}

void fire_on_handshake_complete(struct rasta_notification_result result) {
    UNUSED(result);

    // if (result.handle->notifications.on_handshake_complete == NULL) {
    //     // notification not set, do nothing
    //     return;
    // }

    // on_handshake_complete_call(&result);
}

void on_heartbeat_timeout_call(struct rasta_notification_result *container) {
    UNUSED(container);
    // struct rasta_notification_result *result = (struct rasta_notification_result *)container;

    // (*result->handle->notifications.on_heartbeat_timeout)(result);
}

void fire_on_heartbeat_timeout(struct rasta_notification_result result) {
    UNUSED(result);

    // if (result.handle->notifications.on_heartbeat_timeout == NULL) {
    //     // notification not set, do nothing
    //     return;
    // }

    // on_heartbeat_timeout_call(&result);
}

void rasta_handle_init(struct rasta_handle *h, rasta_config_info *config, struct logger_t *logger) {
    h->config = config;
    h->logger = logger;

    // set notification pointers to NULL
    h->notifications.on_receive = NULL;
    h->notifications.on_connection_state_change = NULL;
    h->notifications.on_diagnostic_notification = NULL;
    h->notifications.on_disconnection_request_received = NULL;
    h->notifications.on_redundancy_diagnostic_notification = NULL;
}
