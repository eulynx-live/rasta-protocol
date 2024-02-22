#pragma once

#include <rasta/config.h>

#include "../util/rastadeferqueue.h"

typedef struct redundancy_mux redundancy_mux;
typedef struct rasta_connection rasta_connection;
typedef struct rasta_transport_channel rasta_transport_channel;
struct rasta_handle;
struct logger_t;

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
     * configuration parameters of the redundancy layer
     */
    rasta_config_redundancy configuration_parameters;

    /**
     * Hashing context for en/decoding SR layer PDUs
     */
    rasta_hashing_context_t hashing_context;
} rasta_redundancy_channel;

/**
 * connects a given redundancy channel on a given connection and multiplexer.
 * @param mux the multiplexer to which the redundancy channel belongs
 * @param channel the redundancy channel to connect
 */
int redundancy_channel_connect(redundancy_mux *mux, rasta_redundancy_channel *channel);

/**
 * close an existing redundancy channel by closing all its transport channels
 * @param c the RaSTA redundancy channel to close
 */
void redundancy_channel_close(rasta_connection *conn, rasta_redundancy_channel *red_channel);

/**
 * initializes a new RaSTA redundancy channel
 * @param h the RaSTA handle to initialite the channel with
 * @param logger the logger that is used to log information
 * @param config the configuration for the redundancy layer
 * @param channel the redundancy channel to initialize
 */
void redundancy_channel_alloc(struct rasta_handle *h, struct logger_t *logger, const rasta_config_info *config, rasta_redundancy_channel *channel);

void redundancy_channel_init(rasta_redundancy_channel *channel);

/**
 * frees memory for the @p channel
 * @param channel the channel that is freed
 */
void redundancy_channel_free(rasta_redundancy_channel *channel);
