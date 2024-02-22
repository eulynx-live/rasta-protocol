#pragma once

typedef struct rasta_redundancy_channel rasta_redundancy_channel;

/**
 * representation of the transport channel diagnostic data
 */
typedef struct {
    /**
     * time (in ms since 1.1.1970) when the current diagnose window was started
     */
    unsigned long start_time;

    /**
     * amount of missed or late messages (late means more than T_SEQ later than on fastest channel)
     */
    int n_missed;

    /**
     * average delay, as described in 6.6.3.2 (2)
     */
    unsigned long t_drift;

    /**
     * quadratic delay, as described in 6.6.3.2 (2)
     */
    unsigned long t_drift2;

    /**
     * amount of packets that are received within the current diagnose window
     */
    int received_packets;
} rasta_redundancy_diagnostics_data;

void run_channel_diagnostics(rasta_redundancy_channel *current, unsigned int transport_channel_index);
