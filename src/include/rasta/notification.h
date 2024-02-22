#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <stddef.h>

/**
 * struct that is returned in all notifications
 */
struct rasta_notification_result {
    /**
     * copy of the calling rasta connection (this should always be used first)
     */
    // struct rasta_connection connection;
    void *nothing;
};

/**
 * pointer to a function that will be called when application messages are ready for processing
 * first parameter is the connection that fired the event
 */
typedef void (*on_receive_ptr)(struct rasta_notification_result *result);

/**
 * pointer to a function that will be called when connection state has changed
 * first parameter is the connection that fired the event
 */
typedef void (*on_connection_state_change_ptr)(struct rasta_notification_result *result);

/**
 * pointer to a function that will be called when diagnostic notification will be send
 * first parameter is the connection that fired the event
 * second parameter is the length for the provided array
 * third parameter it the array with tracked diagnostic data
 */
typedef void (*on_diagnostic_notification_ptr)(struct rasta_notification_result *result);

/**
 * pointer to a function that will be called when a DiscReq are received
 * first parameter is the connection that fired the event.
 * second parameter is the reason for this DiscReq
 * third parameter is the detail for this DiscReq
 */
typedef void (*on_disconnection_request_received_ptr)(struct rasta_notification_result *result, unsigned short reason, unsigned short detail);

/**
 * pointer to a function that will be called when an entity successfully completed the handshake and is now in state UP.
 * first parameter is the connection that fired the event
 */
typedef void (*on_handshake_complete_ptr)(struct rasta_notification_result *);

/**
 * pointer to a function that will be called when the T_i timer of an entity expired.
 * first parameter is the connection that fired the event
 */
typedef void (*on_heartbeat_timeout_ptr)(struct rasta_notification_result *);

typedef struct redundancy_mux redundancy_mux;
struct receive_event_data;
struct sockaddr_in;

/**
 * pointer to a function that will be called in a separate thread when a new entity has sent data to this entity
 * first parameter is the redundancy multiplexer that fired the event
 * 2nd parameter is id of the new redundancy channel / entity
 */
typedef void (*on_new_connection_ptr)(redundancy_mux *, unsigned long);

/**
 * pointer to a function that will be called in a separate thread when diagnostic data is available
 * first parameter is the redundancy multiplexer that fired the event
 * 2nd parameter is N_Diagnose
 * 3rd parameter is N_missed
 * 4th parameter is T_drift
 * 5th parameter is T_drift2
 * 6th parameter is id of redundancy channel where the notification originates
 */
typedef void (*on_diagnostics_available_red_ptr)(redundancy_mux *, int, int, unsigned long, unsigned long,
                                                 unsigned long);

/**
 * function pointers for the notifications that are specified in 5.2.2
 */
struct rasta_notification_ptr {
    /**
     * called when a application message is ready for processing
     */
    on_receive_ptr on_receive;

    /**
     * called when connection state has changed
     */
    on_connection_state_change_ptr on_connection_state_change;

    /**
     * called when diagnostic notification will be send
     */
    on_diagnostic_notification_ptr on_diagnostic_notification;

    /**
     * called when a DiscReq are received
     */
    on_disconnection_request_received_ptr on_disconnection_request_received;

    /**
     * called when a diagnostic notification of the redundancy layer occures
     */
    on_diagnostics_available_red_ptr on_redundancy_diagnostic_notification;

    /**
     * called when an entity successfully completed the handshake and is now in state UP
     */
    on_handshake_complete_ptr on_handshake_complete;

    /**
     * called when the T_i timer of an entity expired
     */
    on_heartbeat_timeout_ptr on_heartbeat_timeout;
};

struct rasta_disconnect_notification_result {
    struct rasta_notification_result result;
    unsigned short reason;
    unsigned short detail;
};

/**
 * representation of the notifications that are available for the redundancy layer
 */
typedef struct
{
    /**
     * the assigned function is called when diagnose information of a transport channel is available.
     * The function will be called in a separate thread
     */
    on_diagnostics_available_red_ptr on_diagnostics_available;

    /**
     * the assigned function is called when a new redundancy channel has sent data to this entity.
     * The function will be called in a separate thread
     */
    on_new_connection_ptr on_new_connection;
} rasta_redundancy_notifications;

#ifdef __cplusplus
}
#endif
