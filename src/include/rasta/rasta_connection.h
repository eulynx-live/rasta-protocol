#pragma once

#include <stdbool.h>

#include "config.h"
#include "event_system.h"
#include "logging.h"
#include "rastafactory.h"
#include "rastahashing.h"
#include "rastaredundancy.h"
#include "rastarole.h"

/**
 * representation of the connection state in the SR layer
 */
typedef enum {
    /**
     * The connection is closed
     */
    RASTA_CONNECTION_CLOSED,
    /**
     * OpenConnection was called, the connection is ready to be established
     */
    RASTA_CONNECTION_DOWN,
    /**
     * In case the role is client: a ConReq was sent, waiting for ConResp
     * In case the role is server: a ConResp was sent, waiting for HB
     */
    RASTA_CONNECTION_START,
    /**
     * Waiting for Key Exchange Request
     */
    RASTA_CONNECTION_KEX_REQ,
    /**
     * Waiting for Key Exchange Response
     */
    RASTA_CONNECTION_KEX_RESP,
    /**
     * Waiting for Key Exchange Authentication
     */
    RASTA_CONNECTION_KEX_AUTH,
    /**
     * The connection was established, ready to send data
     */
    RASTA_CONNECTION_UP,
    /**
     * Retransmission requested
     * RetrReq was sent, waiting for RetrResp
     */
    RASTA_CONNECTION_RETRREQ,
    /**
     * Retransmission running
     * After receiving the RetrResp, resend data will be accepted until HB or regular data arrives
     */
    RASTA_CONNECTION_RETRRUN
} rasta_sr_state;

/**
 * representation of the RaSTA error counters, as specified in 5.5.5
 */
struct rasta_error_counters {
    /**
     * received message with faulty checksum
     */
    unsigned int safety;

    /**
     * received message with unreasonable sender/receiver
     */
    unsigned int address;

    /**
     * received message with undefined type
     */
    unsigned int type;

    /**
     * received message with unreasonable sequence number
     */
    unsigned int sn;

    /**
     * received message with unreasonable confirmed sequence number
     */
    unsigned int cs;
};

/**
 * Representation of a sub interval defined in 5.5.6.4 and used to diagnose healthiness of a connection
 */
struct diagnostic_interval {
    /**
     * represents the start point of this interval.
     * an interval lies between 0 to T_MAX
     */
    unsigned int interval_start;
    /**
     * represents the end point for this interval
     * an interval lies between 0 to T_MAX
     */
    unsigned int interval_end;

    /**
     * counts successful reached messages that lies between current interval_start and interval_end
     */
    unsigned int message_count;
    /**
     * counts every message assigned to this interval that's T_ALIVE value lies between this interval, too
     */
    unsigned int t_alive_message_count;
};

/**
 * The data that is passed to most timed events.
 */
struct timed_event_data {
    void *handle;
    struct rasta_connection *connection;
};

typedef struct rasta_sending_handle {
    /**
     * configuration values
     */
    rasta_config_sending *config;
    rasta_config_general *info;

    struct logger_t *logger;

    struct redundancy_mux *mux;

    /**
     * handle for notification only
     */
    // struct rasta_handle *handle;

    timed_event send_event;

    /**
     * The paramenters that are used for SR checksums
     */
    rasta_hashing_context_t *hashing_context;

    rasta_connection *connection;
} rasta_sending_handle;

typedef struct rasta_heartbeat_handle {

    struct logger_t *logger;

    struct redundancy_mux *mux;

    /**
     * handle for notification only
     */
    // struct rasta_handle *handle;

    /**
     * The paramenters that are used for SR checksums
     */
    rasta_hashing_context_t *hashing_context;
} rasta_heartbeat_handle;

typedef struct rasta_receive_handle {
    /**
     * configuration values
     */
    rasta_config_sending *config;
    rasta_config_general *info;

    struct logger_t *logger;

    rasta_connection *connection;

    /**
     * handle for notification only
     */
    struct rasta_handle *handle;

    /**
     * The paramenters that are used for SR checksums
     */
    rasta_hashing_context_t *hashing_context;
} rasta_receive_handle;

typedef struct rasta_connection {

    // Flag to tell the caller that the connection was established in the last event loop run
    bool is_new;

    timed_event handshake_timeout_event;

    /**
     * the event operating the heartbeats on this connection
     */
    timed_event send_heartbeat_event;
    struct timed_event_data heartbeat_carry_data;

    /**
     * the event watching the connection timeout
     */
    timed_event timeout_event;
    struct timed_event_data timeout_carry_data;

#ifdef ENABLE_OPAQUE
    /**
     * triggers new key exchange
     */
    timed_event rekeying_event;
    struct timed_event_data rekeying_carry_data;
#endif

    /**
     * blocks heartbeats until connection handshake is complete
     */
    int hb_locked;

    rasta_sr_state current_state;

    /**
     * the name of the sending message queue
     */
    fifo_t *fifo_send;

    /**
     * the N_SENDMAX of the connection partner,  -1 if not connected
     */
    int connected_recv_buffer_size;

    /**
     * defines if who started the connection
     * Client: Connection request sent
     * Server: Connection request received
     */
    rasta_role role;

    /**
     * send sequence number (seq nr of the next PDU that will be sent)
     */
    uint32_t sn_t;
    /**
     * receive sequence number (expected seq nr of the next received PDU)
     */
    uint32_t sn_r;

    /**
     * Initial sequence number
     */
    uint32_t sn_i;

    /**
     * sequence number that has to be checked in the next sent PDU
     */
    uint32_t cs_t;
    /**
     * last received, checked sequence number
     */
    uint32_t cs_r;

    /**
     * timestamp of the last received relevant message
     */
    uint32_t ts_r;
    /**
     * checked timestamp of the last received relevant message
     */
    uint32_t cts_r;

    /**
     * relative time used to monitor incoming messages
     */
    unsigned int t_i;

    /**
     * the RaSTA connections sender identifier
     */
    uint32_t my_id;
    /**
     * the RaSTA connections receiver identifier
     */
    uint32_t remote_id;
    /**
     * the identifier of the RaSTA network this connection belongs to
     */
    uint32_t network_id;

    /**
     * counts received diagnostic relevant messages since last diagnosticNotification
     */
    unsigned int received_diagnostic_message_count;
    /**
     * length of diagnostic_intervals array
     */
    unsigned int diagnostic_intervals_length;
    /**
     * diagnostic intervals defined at 5.5.6.4 to diagnose healthiness of this connection
     * number of fields defined by DIAGNOSTIC_INTERVAL_SIZE
     */
    struct diagnostic_interval *diagnostic_intervals;

    /**
     * the pdu fifo for retransmission purposes
     */
    fifo_t *fifo_retransmission;

    /**
     *   the error counters as specified in 5.5.5
     */
    struct rasta_error_counters errors;

    /**
     * Session data for and derived from key exchange
     */
    struct key_exchange_state kex_state;

    rasta_redundancy_channel* redundancy_channel;

    rasta_receive_handle receive_handle;

    rasta_sending_handle send_handle;

    /**
     * the heartbeat data
     */
    rasta_heartbeat_handle heartbeat_handle;

    rasta_config_info *config;

    struct logger_t *logger;
} rasta_connection;

/**
 * struct that is returned in all notifications
 */
struct rasta_notification_result {
    /**
     * copy of the calling rasta connection (this should always be used first)
     */
    struct rasta_connection connection;
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
 * creates the container for all notification events
 * @param handle
 * @param connection
 * @return
 */
struct rasta_notification_result sr_create_notification_result(struct rasta_handle *handle, struct rasta_connection *connection);

/**
 * fires the onConnectionStateChange event set in the rasta handle
 * @param result
 */
void fire_on_connection_state_change(struct rasta_notification_result result);

/**
 * fires the onReceive event set in the rasta handle
 * @param result
 */
void fire_on_receive(struct rasta_notification_result result);

/**
 * fires the onDisconnectionRequest event set in the rasta handle
 * @param result
 * @param data
 */
void fire_on_discrequest_state_change(struct rasta_notification_result result, struct RastaDisconnectionData data);

/**
 * fires the onDiagnosticAvailable event set in the rasta handle
 * @param result
 */
void fire_on_diagnostic_notification(struct rasta_notification_result result);

/**
 * fires the onHandshakeComplete event set in the rasta handle
 * @param result
 */
void fire_on_handshake_complete(struct rasta_notification_result result);

/**
 * fires the onHeartbeatTimeout event set in the rasta handle
 * @param result
 */
void fire_on_heartbeat_timeout(struct rasta_notification_result result);

