#include "rasta_redundancy_channel.h"

#include "../rasta_connection.h"
#include "../transport/transport.h"
#include "../util/rastadeferqueue.h"
#include "../util/rmemory.h"
#include "rasta_red_multiplexer.h"
#include "rasta_redundancy_channel.h"

int rasta_red_connect_transport_channel(rasta_redundancy_channel *channel, rasta_transport_socket *transport_socket) {
    rasta_transport_channel *transport_connection = &channel->transport_channels[transport_socket->id];
    transport_connect(transport_socket, transport_connection);
    return transport_connection->connected;
}

void redundancy_channel_init(rasta_redundancy_channel *channel) {
    channel->seq_rx = 0;
    channel->seq_tx = 0;

    // init defer queue
    deferqueue_clear(&channel->defer_q);

    // init diagnostics buffer
    deferqueue_clear(&channel->diagnostics_packet_buffer);
}

int redundancy_channel_connect(redundancy_mux *mux, rasta_redundancy_channel *channel) {
    redundancy_channel_init(channel);

    // add transport channels
    bool success = false;
    for (unsigned int i = 0; i < channel->transport_channel_count; i++) {
        // Provided transport channels have to match with local ports configured
        success |= rasta_red_connect_transport_channel(channel, &mux->transport_sockets[i]);
    }

    if (!success) {
        return 1;
    }

    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux add channel", "added new redundancy channel for ID=0x%lX", channel->associated_id);

    return 0;
}

void redundancy_channel_close(rasta_connection *conn, rasta_redundancy_channel *red_channel) {
    for (unsigned int i = 0; i < red_channel->transport_channel_count; ++i) {
        rasta_transport_channel *channel = &red_channel->transport_channels[i];
        logger_log(red_channel->mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "closing transport channel %u/%u", i + 1, red_channel->transport_channel_count);
        transport_close_channel(channel);
        // if we are a TCP/TLS client (and transport_close_channel actually closes the channel), the socket fd also becomes invalid
        if (!channel->connected && conn->role == RASTA_ROLE_CLIENT) {
            channel->associated_socket->file_descriptor = -1;
        }
    }
}

void redundancy_channel_alloc(struct rasta_handle *h, struct logger_t *logger, const rasta_config_info *config, rasta_redundancy_channel *channel) {

    channel->associated_id = config->general.rasta_id_remote;

    channel->logger = logger;
    channel->configuration_parameters = config->redundancy;

    // init sequence numbers
    channel->seq_rx = 0;
    channel->seq_tx = 0;

    // init defer queue
    channel->defer_q = deferqueue_init(config->redundancy.n_deferqueue_size);

    // init diagnostics buffer
    channel->diagnostics_packet_buffer = deferqueue_init(10 * config->redundancy.n_deferqueue_size);

    // init hashing context
    channel->hashing_context.hash_length = config->sending.md4_type;
    channel->hashing_context.algorithm = config->sending.sr_hash_algorithm;

    if (channel->hashing_context.algorithm == RASTA_ALGO_MD4) {
        // use MD4 IV as key
        rasta_md4_set_key(&channel->hashing_context, config->sending.md4_a, config->sending.md4_b,
                          config->sending.md4_c, config->sending.md4_d);
    } else {
        // use the sr_hash_key
        allocateRastaByteArray(&channel->hashing_context.key, sizeof(unsigned int));

        // convert unsigned in to byte array
        channel->hashing_context.key.bytes[0] = (config->sending.sr_hash_key >> 24) & 0xFF;
        channel->hashing_context.key.bytes[1] = (config->sending.sr_hash_key >> 16) & 0xFF;
        channel->hashing_context.key.bytes[2] = (config->sending.sr_hash_key >> 8) & 0xFF;
        channel->hashing_context.key.bytes[3] = (config->sending.sr_hash_key) & 0xFF;
    }

    // init transport channel buffer;
    unsigned int transport_channel_count = config->redundancy_remote.connections.count;
    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red init", "space for %d connected channels", transport_channel_count);
    channel->transport_channels = rmalloc(transport_channel_count * sizeof(rasta_transport_channel));
    rmemset(channel->transport_channels, 0, transport_channel_count * sizeof(rasta_transport_channel));
    channel->transport_channel_count = transport_channel_count;

    for (unsigned i = 0; i < transport_channel_count; i++) {
        transport_init(h, &channel->transport_channels[i], i, config->redundancy_remote.connections.data[i].ip, config->redundancy_remote.connections.data[i].port, &config->tls);
    }
}

void redundancy_channel_free(rasta_redundancy_channel *channel) {
    // destroy the diagnostics buffer
    deferqueue_destroy(&channel->diagnostics_packet_buffer);

    // destroy the defer queue
    deferqueue_destroy(&channel->defer_q);

    // free the channels
    rfree(channel->transport_channels);
    channel->transport_channel_count = 0;

    freeRastaByteArray(&channel->hashing_context.key);

    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red cleanup", "Cleanup complete");
}
