#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include "logging.h"
#include "rastahandle.h"

/**
 * size of ring buffer where data is hold for retransmissions
 */
#define MAX_QUEUE_SIZE 100 // TODO: maybe in config file

#define DIAGNOSTIC_INTERVAL_SIZE 500

struct user_callbacks {
    /**
     * This handler is called in case RaSTA cleanup is called.
     */
    void (*on_rasta_cleanup)();
};

typedef struct rasta_lib_configuration_s {
    struct rasta_handle h;
    event_system rasta_lib_event_system;
    struct user_callbacks callback;
} rasta_lib_configuration_t[1];

typedef struct rasta_connection rasta_lib_connection_t[1];

void init_send_key_exchange_event(timed_event *ev, struct timed_event_data *carry_data,
                                  struct rasta_connection *connection);

/**
 * initializes the RaSTA handle with a given configuration and logger
 * @param user_configuration the user configuration containing the handle to initialize
 * @param config the configuration to initialize the handle with
 * @param logger the logger to use
 */
void rasta_socket(rasta_lib_configuration_t user_configuration, rasta_config_info *config, struct logger_t *logger);

/**
 * initializes the RaSTA handle and all configured connections
 * @param user_configuration the user configuration containing the handle to initialize
 * @param config the configuration to initialize the handle with
 * @param logger the logger to use
 * @param connections the connections to initialize
 * @param connections_length the length of the connections array
 */
void rasta_lib_init_configuration(rasta_lib_configuration_t user_configuration, rasta_config_info *config, struct logger_t *logger, rasta_connection_config *connections, size_t connections_length);

#ifdef __cplusplus
}
#endif
