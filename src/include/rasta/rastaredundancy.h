#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <stdint.h>

#include "config.h"
#include "fifo.h"
#include "logging.h"
#include "rastacrc.h"
#include "rastadeferqueue.h"

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
 * representation of a RaSTA redundancy channel
 */
typedef struct rasta_redundancy_channel {
    struct redundancy_mux *mux;

    /**
     * the RaSTA ID of the remote entity this channel is bound to
     */
    unsigned long associated_id;

    /**
     * the type of checksum that is used for all messages
     */
    struct crc_options checksum_type;

    /**
     * next sequence number to send
     */
    unsigned long seq_tx;

    /**
     * next expected sequence number to be received
     */
    unsigned long seq_rx;

    /**
     * the defer queue
     */
    struct defer_queue defer_q;

    /**
     * used to store all received packets within a diagnose window
     */
    struct defer_queue diagnostics_packet_buffer;

    /**
     * the transport channels of the partner (client) when running in server mode.
     * these are dynamically initialzed when a message from the corresponding channel is received.
     */
    rasta_transport_channel *transport_channels;

    /**
     * the total amount of transport channels
     */
    unsigned int transport_channel_count;

    /**
     * logger used for logging
     */
    struct logger_t *logger;

    /**
     * 1 if the redundancy channel is open, 0 if it is closed
     */
    int is_open;

    /**
     * configuration parameters of the redundancy layer
     */
    rasta_config_redundancy configuration_parameters;

    /**
     * Hashing context for en/decoding SR layer PDUs
     */
    rasta_hashing_context_t hashing_context;
} rasta_redundancy_channel;

/**
 * initializes a new RaSTA redundancy channel
 * @param h the RaSTA handle to initialite the channel with
 * @param logger the logger that is used to log information
 * @param config the configuration for the redundancy layer
 * @param transport_sockets the transport sockets belonging to this redundancy channel
 * @param transport_channel_count the amount of transport channels that should be created in this redundancy channel (corresponds to the number of transport sockets supplied)
 * @param id the RaSTA ID that is associated with this redundancy channel
 * @param channel the redundancy channel to initialize
 */
void red_f_init(struct rasta_handle *h, struct logger_t *logger, const rasta_config_info *config, rasta_ip_data *transport_sockets, unsigned int transport_channel_count,
                unsigned long id, rasta_redundancy_channel *channel);

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

/**
 * connects the transport channel of the given redundancy @p channel which corresponds to the supplied @p transport_socket, on the given @p connection
 */
int rasta_red_connect_transport_channel(rasta_connection *h, rasta_redundancy_channel *channel, rasta_transport_socket *transport_socket);

/**
 * frees memory for the @p channel
 * @param channel the channel that is freed
 */
void red_f_cleanup(rasta_redundancy_channel *channel);

#ifdef __cplusplus
}
#endif
