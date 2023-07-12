#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <stdbool.h>

#include "config.h"
#include "logging.h"
#include "rasta_red_multiplexer.h"
#include "rastafactory.h"
#include "rastahashing.h"
#include "rasta_connection.h"

#ifdef ENABLE_OPAQUE
#include <opaque.h>
#endif


typedef struct rasta_handle {

    /**
     * pointers to functions that will be called on notifications as described in 5.2.2 and 5.5.6.4
     */
    struct rasta_notification_ptr notifications;

    /**
     * the logger which is used to log protocol activities
     */
    struct logger_t *logger;

    /**
     * RaSTA parameters
     */
    rasta_config_info *config;

    /**
     * versions that this RaSTA entity will accept during the handshake
     */
    // struct DictionaryArray accepted_version;

    /**
     * provides access to the redundancy layer
     */
    struct redundancy_mux mux;

    /**
     * the global event system on the main thread
     */
    event_system *ev_sys;

    /**
     * the user specified configurations for RaSTA
     */
    struct user_callbacks *user_handles;

    rasta_connection *rasta_connections;
    unsigned rasta_connections_length;

    struct rasta_connection *accepted_connection;
} rasta_handle;

/**
 * initializes the rasta handle
 * configures itself with the given config and logger automatically
 * @param h
 * @param config
 * @param logger
 */
void rasta_handle_init(struct rasta_handle *h, rasta_config_info *config, struct logger_t *logger);

#ifdef __cplusplus
}
#endif
