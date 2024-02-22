#pragma once

#include <arpa/inet.h>
#include <stdint.h>

#include <rasta/notification.h>
#include <rasta/rastarole.h>

#include "../util/event_system.h"
#include "../util/rastamodule.h"
#include "rasta_redundancy_channel.h"
#include "rastaredundancy.h"

#define UNUSED(x) (void)(x)

typedef struct redundancy_mux redundancy_mux;
typedef struct rasta_connection rasta_connection;
struct receive_event_data;
struct rasta_handle;

/**
 * initialize the event handling channel timeouts and the corresponding carry data
 * @param event the event to initialize
 * @param channel_timeout_ms the timeout (in ms) to initialize the event with
 */
void init_handshake_timeout_event(timed_event *event, int channel_timeout_ms);

/**
 * representation of a redundancy layer multiplexer.
 * is used to handle multiple redundancy channels.
 */
struct redundancy_mux {
    /**
     * the ports where this entity will listen (where the udp sockets are bound)
     */
    uint16_t *listen_ports;

    /**
     * amount of listen ports, i.e. length of the listen_ports array
     */
    unsigned int port_count;

    /**
     * the rasta transport state of each used socket. The array has a length of port_count
     */
    rasta_transport_socket *transport_sockets;

    /**
     * the redundancy channels to remote entities this multiplexer is aware of
     */
    rasta_redundancy_channel *redundancy_channel;

    /**
     * the logger that is used to log information
     */
    struct logger_t *logger;

    /**
     * configuration data for the multiplexer and redundancy channels
     */
    rasta_config_info *config;

    /**
     * the notifications of this multiplexer and it's redundancy channels
     */
    rasta_redundancy_notifications notifications;

    /**
     * amount of notification thread that are currently running
     */
    unsigned int notifications_running;

    /**
     * Hashing paramenter for SR layer checksum
     */
    rasta_hashing_context_t sr_hashing_context;
};

/**
 * initializes an redundancy layer multiplexer. The ports and interfaces to listen on are read from the config.
 * @param mux the redundancy layer multiplexer to initialize
 * @param logger the logger that is used to log information
 * @param config configuration for redundancy channels
 */
void redundancy_mux_alloc(struct rasta_handle *h, redundancy_mux *mux, struct logger_t *logger, rasta_config_info *config);

/**
 * binds all transport sockets of a redundancy layer multiplexer to their respective IP/port
 * @param h the RaSTA handle containing the multiplexer
 */
bool redundancy_mux_bind(struct rasta_handle *h);

/**
 * stops the redundancy layer multiplexer and closes all redundancy channels before cleaning up memory
 * @param mux the multiplexer that will be closed
 */
void redundancy_mux_close(redundancy_mux *mux);

/**
 * send a RaSTA packet on a given redundancy channel
 * @param channel the redundancy channel to send on
 * @param data the packet to send
 * @param role whether to send as a client or server
 */
void redundancy_mux_send(rasta_redundancy_channel *channel, struct RastaPacket *data, rasta_role role);

/**
 * listen on all transport sockets of the given multiplexer
 * @param mux the mux used for listening
 */
void redundancy_mux_listen_channels(redundancy_mux *mux);

void redundancy_mux_init(redundancy_mux *mux);

// handlers
int receive_packet(redundancy_mux *mux, rasta_transport_channel *channel, unsigned char *buffer, size_t len);
void handle_received_data(redundancy_mux *mux, unsigned char *buffer, ssize_t len, struct RastaRedundancyPacket *receivedPacket);
int handle_closed_transport(rasta_connection *h, rasta_redundancy_channel *channel);
