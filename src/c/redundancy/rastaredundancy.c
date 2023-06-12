#include <rasta/rastaredundancy.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rasta/rastautil.h>
#include <rasta/rmemory.h>
#include <rasta/rastahandle.h>

#include "../retransmission/safety_retransmission.h"
#include "../transport/transport.h"

int _deliver_message_to_upper_layer(rasta_connection *h, rasta_redundancy_channel *channel, struct RastaByteArray message) {
    struct RastaPacket packet;
    bytesToRastaPacket(message, &channel->hashing_context, &packet);
    return sr_receive(h, &packet);
}

void red_f_init(struct rasta_handle *h, struct logger_t *logger, const rasta_config_info *config, rasta_ip_data *transport_sockets, unsigned int transport_channel_count,
                unsigned long id, rasta_redundancy_channel *channel) {

    channel->associated_id = id;

    channel->logger = logger;
    channel->configuration_parameters = config->redundancy;

    channel->is_open = 0;

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
    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red init", "space for %d connected channels", transport_channel_count);
    channel->transport_channels = rmalloc(transport_channel_count * sizeof(rasta_transport_channel));
    rmemset(channel->transport_channels, 0, transport_channel_count * sizeof(rasta_transport_channel));
    channel->transport_channel_count = transport_channel_count;

    for (unsigned j = 0; j < transport_channel_count; j++) {
        transport_init(h, &channel->transport_channels[j], j, transport_sockets[j].ip, transport_sockets[j].port, &config->tls);
    }
}

/**
 * delivers a message in the defer queue to next layer i.e. adds it to the receive buffer
 * see 6.6.4.4.6 for more details
 * @param connection the connection data which is used
 */
int red_f_deliverDeferQueue(rasta_connection *con, rasta_redundancy_channel *channel) {
    int result = 0;
    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red deliver deferq", "f_deliverDeferQueue called");

    // check if message with seq_pdu == seq_rx in defer queue
    while (deferqueue_contains(&channel->defer_q, channel->seq_rx)) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red deliver deferq", "deferq contains seq_pdu=%lu",
                   channel->seq_rx);

        struct RastaByteArray innerPackerBytes;
        // convert inner data (RaSTA SR layer PDU) to byte array
        struct RastaRedundancyPacket queuePacket = deferqueue_get(&channel->defer_q, channel->seq_rx);
        innerPackerBytes = rastaModuleToBytes(&queuePacket.data, &channel->hashing_context);
        result |= _deliver_message_to_upper_layer(con, channel, innerPackerBytes);
        freeRastaByteArray(&innerPackerBytes);

        // remove message from queue (effectively a pop operation with the get call)
        deferqueue_remove(&channel->defer_q, channel->seq_rx);
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red deliver deferq", "remove message from deferq");

        // increase seq_rx
        channel->seq_rx++;
    }
    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red deliver deferq", "deferq doesn't contain seq_pdu=%lu",
               channel->seq_rx);
    return result;
}

int red_f_receiveData(rasta_redundancy_channel *channel, struct RastaRedundancyPacket packet, int channel_id) {
    int result = 0;

    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "Channel %d: ptr=%p", channel_id, (void *)channel);

    if (!packet.checksum_correct) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "Channel 0: Packet checksum incorrect on channel %d", channel_id);

        // checksum incorrect, exit function
        return 0;
    }

    // else checksum correct

    { // Diagnostics
        // increase amount of received packets of this channel
        channel->transport_channels[channel_id].diagnostics_data.received_packets += 1;
    }

    // only accept pdu with seq. nr = 0 as first message
    if (channel->seq_rx == 0 && channel->seq_tx == 0 && packet.sequence_number != 0) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: first seq_pdu != 0", channel_id);

        return 0;
    }

    // check seq_pdu
    if (packet.sequence_number < channel->seq_rx) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: seq_pdu < seq_rx", channel_id);
        // message has been received by other transport channel

        { // Diagnostics
            // -> calculate delay by looking for the received ts in diagnostics queue

            unsigned long ts = deferqueue_get_ts(&channel->diagnostics_packet_buffer, packet.sequence_number);
            if (ts != 0) {
                // seq_pdu was in queue, received time is ts
                unsigned long delay = cur_timestamp() - ts;

                // if delay > T_SEQ, message is late
                if (delay > channel->configuration_parameters.t_seq) {
                    // channel is late, increase missed counter
                    channel->transport_channels[channel_id].diagnostics_data.n_missed++;
                } else {
                    // update t_drift and t_drift2
                    channel->transport_channels[channel_id].diagnostics_data.t_drift += delay;
                    channel->transport_channels[channel_id].diagnostics_data.t_drift2 += (delay * delay);
                }
            }
        }

        // discard message
        return 0;
    } else if (packet.sequence_number == channel->seq_rx) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: correct seq. nr. delivering to next layer",
                   channel_id);
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: seq_pdu=%lu, seq_rx=%lu",
                   channel_id, (long unsigned int)packet.sequence_number, channel->seq_rx - 1);

        { // Diagnostics
            // received packet as first transport channel -> add with ts to diagnostics buffer
            if (!deferqueue_add(&channel->diagnostics_packet_buffer, packet, cur_timestamp())) {
                logger_log(channel->logger, LOG_LEVEL_INFO, "RaSTA Red receive", "diagnostics packet buffer is full");
            }
        }

        // Here we deviate a bit from the flow diagram in the docs, because also insert in-order
        // packets into the defer queue. This is to support the case where we receive data from
        // connections that we don't currently rasta_recv from

        if (!deferqueue_add(&channel->defer_q, packet, cur_timestamp())) {
            logger_log(channel->logger, LOG_LEVEL_INFO, "RaSTA Red receive", "discarded packet because defer queue was full");
        }

        // struct RastaByteArray innerPacketBytes;
        // // convert inner data (RaSTA SR layer PDU) to byte array
        // innerPacketBytes = rastaModuleToBytesNoChecksum(&packet.data, &channel->hashing_context);
        // result = _deliver_message_to_upper_layer(connection, channel, innerPacketBytes);
        // freeRastaByteArray(&innerPacketBytes);

        // logger_log(connection->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: added message to buffer",
        //            channel_id);

        // increase seq_rx
        // channel->seq_rx++;

        // deliver already received messages to upper layer
        // result |= red_f_deliverDeferQueue(h, channel);
        return 1;
    } else if (channel->seq_rx < packet.sequence_number && packet.sequence_number <= (channel->seq_rx + channel->configuration_parameters.n_deferqueue_size * 10)) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: seq_rx < seq_pdu && seq_pdu <= (seq_rx + 10 * MAX_DEFERQUEUE_SIZE)", channel_id);

        // check if message is in defer queue
        if (deferqueue_contains(&channel->defer_q, packet.sequence_number)) {
            logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: packet already in deferq",
                       channel_id);

            // discard message
            // possibly statistic analysis
            return 0;
        } else {
            // check if queue is full
            if (deferqueue_isfull(&channel->defer_q)) {
                logger_log(channel->logger, LOG_LEVEL_INFO, "RaSTA Red receive", "channel %d: deferq full", channel_id);

                // full -> discard message
                return 0;
            } else {
                logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: adding message to deferq",
                           channel_id);

                // add message to defer queue
                if (!deferqueue_add(&channel->defer_q, packet, cur_timestamp())) {
                    logger_log(channel->logger, LOG_LEVEL_INFO, "RaSTA Red receive", "discarded packet because defer queue was full");
                }
            }
        }
    } else if (packet.sequence_number > (channel->seq_rx + channel->configuration_parameters.n_deferqueue_size * 10)) {
        logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red receive", "channel %d: seq_pdu > seq_rx + 10 * MAX_DEFERQUEUE_SIZE", channel_id);

        // discard message
        return 0;
    }

    return result;
}

void red_f_deferTmo(rasta_connection *h, rasta_redundancy_channel *channel) {
    // find smallest seq_pdu in defer queue
    int smallest_index = deferqueue_smallest_seqnr(&channel->defer_q);

    // set seq_rx to it
    channel->seq_rx = channel->defer_q.elements[smallest_index].packet.sequence_number;

    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red f_deferTmo", "calling f_deliverDeferQueue");
    red_f_deliverDeferQueue(h, channel);
}

void red_f_cleanup(rasta_redundancy_channel *channel) {
    // destroy the defer queue
    // deferqueue_destroy(&channel->defer_q);

    // destroy the diagnostics buffer
    // deferqueue_destroy(&channel->diagnostics_packet_buffer);

    // free the channels
    // rfree(channel->transport_channels);
    // channel->transport_channel_count = 0;

    // freeRastaByteArray(&channel->hashing_context.key);

    logger_log(channel->logger, LOG_LEVEL_DEBUG, "RaSTA Red cleanup", "Cleanup complete");
}
