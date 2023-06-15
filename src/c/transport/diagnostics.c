#include "diagnostics.h"

#include "transport.h"

/**
 * wrapper for parameter in the diagnose notification thread handler
 */
struct diagnose_notification_parameter_wrapper {
    /**
     * the used redundancy multiplexer
     */
    redundancy_mux *mux;

    /**
     * value of N_diagnose
     */
    int n_diagnose;

    /**
     * current value of N_missed
     */
    int n_missed;

    /**
     * current value of T_drift
     */
    unsigned long t_drift;

    /**
     * current value of T_drift2
     */
    unsigned long t_drift2;

    /**
     * associated id of the redundancy channel this notification origins from
     */
    unsigned long channel_id;
};

/**
 * the is the function that handles the call of the onDiagnosticsAvailable notification pointer.
 * this runs on the main thread
 * @param wrapper a wrapper that contains the mux and the diagnose data
 * @return unused
 */
void red_on_diagnostic_caller(struct diagnose_notification_parameter_wrapper *w) {
    logger_log(w->mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy onDiagnostics caller", "calling onDiagnostics function");
    (*w->mux->notifications.on_diagnostics_available)(w->mux, w->n_diagnose, w->n_missed, w->t_drift, w->t_drift2, w->channel_id);

    w->mux->notifications_running = (unsigned short)(w->mux->notifications_running - 1);
}

/**
 * fires the onDiagnoseAvailable event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param mux the redundancy multiplexer that is used
 * @param n_diagnose the value of N_Diagnose
 * @param n_missed the current value of N_missed
 * @param t_drift the current value of T_drift
 * @param t_drift2 the current value of T_drift2
 * @param id the id of the redundancy channel
 */
void red_call_on_diagnostic(redundancy_mux *mux, int n_diagnose,
                            int n_missed, unsigned long t_drift, unsigned long t_drift2, unsigned long id) {
    if (mux->notifications.on_diagnostics_available == NULL) {
        // notification not set, do nothing
        return;
    }

    mux->notifications_running++;

    struct diagnose_notification_parameter_wrapper wrapper;
    wrapper.mux = mux;
    wrapper.n_diagnose = n_diagnose;
    wrapper.n_missed = n_missed;
    wrapper.t_drift = t_drift;
    wrapper.t_drift2 = t_drift2;
    wrapper.channel_id = id;

    red_on_diagnostic_caller(&wrapper);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy call onDiagnostics", "called onDiagnostics");
}

void run_channel_diagnostics(rasta_redundancy_channel* current, unsigned int transport_channel_index) {
    int n_diagnose = current->mux->config->redundancy.n_diagnose;

    unsigned long channel_diag_start_time = current->transport_channels[transport_channel_index].diagnostics_data.start_time;

    if (cur_timestamp() - channel_diag_start_time >= (unsigned long)n_diagnose) {
        // increase n_missed by amount of messages that are not received

        // amount of missed packets
        int missed_count = current->diagnostics_packet_buffer.count -
                            current->transport_channels[transport_channel_index].diagnostics_data.received_packets;

        // increase n_missed
        current->transport_channels[transport_channel_index].diagnostics_data.n_missed += missed_count;

        // window finished, fire event
        // fire diagnostic notification
        red_call_on_diagnostic(current->mux,
                                current->mux->config->redundancy.n_diagnose,
                                current->transport_channels[transport_channel_index].diagnostics_data.n_missed,
                                current->transport_channels[transport_channel_index].diagnostics_data.t_drift,
                                current->transport_channels[transport_channel_index].diagnostics_data.t_drift2,
                                current->associated_id);

        // reset values
        current->transport_channels[transport_channel_index].diagnostics_data.n_missed = 0;
        current->transport_channels[transport_channel_index].diagnostics_data.received_packets = 0;
        current->transport_channels[transport_channel_index].diagnostics_data.t_drift = 0;
        current->transport_channels[transport_channel_index].diagnostics_data.t_drift2 = 0;
        current->transport_channels[transport_channel_index].diagnostics_data.start_time = cur_timestamp();

        deferqueue_clear(&current->diagnostics_packet_buffer);
    }
}
