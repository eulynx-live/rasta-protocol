#pragma once

#include <stdint.h>

#include <rasta/config.h>

#include "../logging.h"
#include "../util/fifo.h"
#include "../util/rastacrc.h"
#include "../util/rastadeferqueue.h"

typedef struct rasta_redundancy_channel rasta_redundancy_channel;
typedef struct rasta_transport_channel rasta_transport_channel;
typedef struct rasta_transport_socket rasta_transport_socket;
typedef struct rasta_receive_handle rasta_receive_handle;
typedef struct rasta_sending_handle rasta_sending_handle;
typedef struct rasta_heartbeat_handle rasta_heartbeat_handle;
typedef struct rasta_connection rasta_connection;
struct rasta_handle;

/**
 * maximum size of messages in the defer queue in bytes
 */
#define MAX_DEFER_QUEUE_MSG_SIZE 1000

/**
 * representation of the state of a redundancy channel as defined in 6.6.4.1
 */
typedef enum {
    /**
     * Redundancy channel is up
     */
    RASTA_RED_UP,

    /**
     * redundancy channel is down
     */
    RASTA_RED_CLOSED
} rasta_redundancy_state;

/**
 * the f_receive function of the redundancy layer
 * @param channel the redundancy channel that is used
 * @param packet the packet that has been received over UDP
 * @param channel_id the index of the transport channel, the @p packet has been received
 */
int red_f_receiveData(rasta_redundancy_channel *channel, struct RastaRedundancyPacket packet, int channel_id);
int red_f_deliverDeferQueue(rasta_connection *con, rasta_redundancy_channel *channel);

/**
 * the f_deferTmo function of the redundancy layer
 * @param h the RaSTA connection that is used
 * @param channel the redundancy channel that is used
 */
void red_f_deferTmo(rasta_connection *h, rasta_redundancy_channel *channel);
