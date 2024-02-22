#pragma once

#include <stdbool.h>

#include <rasta/config.h>
#include <rasta/notification.h>

#include "logging.h"
#include "rastafactory.h"
#include "redundancy/rasta_red_multiplexer.h"
#include "util/event_system.h"
#include "util/rastahashing.h"

#ifdef ENABLE_OPAQUE
#include <opaque.h>
#endif

typedef struct rasta_connection rasta_connection;

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
     * provides access to the redundancy layer
     */
    struct redundancy_mux mux;

    /**
     * the global event system on the main thread
     */
    event_system *ev_sys;

    rasta_connection *rasta_connection;

    struct rasta_connection *accepted_connection;
} rasta_handle;

typedef struct rasta {
    struct logger_t logger;
    struct rasta_handle h;
    event_system rasta_lib_event_system;
} rasta;

typedef struct rasta_sending_handle {
    /**
     * configuration values
     */
    rasta_config_sending *config;
    rasta_config_general *info;

    struct logger_t *logger;

    struct redundancy_mux *mux;

    /**
     * handle for notification only
     */
    // struct rasta_handle *handle;

    timed_event send_event;

    /**
     * The paramenters that are used for SR checksums
     */
    rasta_hashing_context_t *hashing_context;

    rasta_connection *connection;
} rasta_sending_handle;

typedef struct rasta_heartbeat_handle {

    struct logger_t *logger;

    struct redundancy_mux *mux;

    // struct rasta_handle *handle; // handle for notification only

    /**
     * The parameters that are used for SR checksums
     */
    rasta_hashing_context_t *hashing_context;
} rasta_heartbeat_handle;

typedef struct rasta_receive_handle {
    /**
     * configuration values
     */
    rasta_config_sending *config;
    rasta_config_general *info;

    struct logger_t *logger;

    rasta_connection *connection;

    /**
     * handle for notification only
     */
    struct rasta_handle *handle;

    /**
     * The paramenters that are used for SR checksums
     */
    rasta_hashing_context_t *hashing_context;
} rasta_receive_handle;

/**
 * initializes the rasta handle
 * configures itself with the given config and logger automatically
 * @param h
 * @param config
 * @param logger
 */
void rasta_handle_init(struct rasta_handle *h, rasta_config_info *config, struct logger_t *logger);
