#pragma once

struct rasta_connection;

/**
 * Reasons for DiscReq as specified in 5.4.6
 */
typedef enum {
    /**
     * Disconnection because of user request
     */
    RASTA_DISC_REASON_USERREQUEST = 0,
    /**
     * Disconnection because of receiving an unexpected type of packet
     */
    RASTA_DISC_REASON_UNEXPECTEDTYPE = 2,
    /**
     * Disconnection because of an error in the sequence number check
     */
    RASTA_DISC_REASON_SEQNERROR = 3,
    /**
     * Disconnection because of a timeout
     */
    RASTA_DISC_REASON_TIMEOUT = 4,
    /**
     * Disconnection because of the call of the service was not allowed
     */
    RASTA_DISC_REASON_SERVICENOTALLOWED = 5,
    /**
     * Disconnection because of the version was not accepted
     */
    RASTA_DISC_REASON_INCOMPATIBLEVERSION = 6,
    /**
     * Disconnection because retransmission failed
     */
    RASTA_DISC_REASON_RETRFAILED = 7,
    /**
     * Disconnection because an error in the protocol flow
     */
    RASTA_DISC_REASON_PROTOCOLERROR = 8
} rasta_disconnect_reason;

void sendDisconnectionRequest(struct rasta_connection *connection, rasta_disconnect_reason reason, unsigned short details);
void sendHeartbeat(struct rasta_connection *connection, char reschedule_manually);
void sendRetransmissionRequest(struct rasta_connection *connection);
void sendRetransmissionResponse(struct rasta_connection *connection);
