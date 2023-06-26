#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include "logging.h"
#include "rasta.h"
#include "rastahandle.h"

// The header, which the user will include later.

typedef struct rasta_connection rasta_lib_connection_t[1];

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

typedef fd_event rasta_lib_fd_event;
typedef timed_event rasta_lib_timed_event;

void rasta_lib_init_configuration(rasta_lib_configuration_t user_configuration, rasta_config_info *config, struct logger_t *logger, rasta_connection_config *connections, size_t connections_length);

struct rasta_connection * rasta_accept(rasta_lib_configuration_t user_configuration);
int rasta_recv(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len);
int rasta_send(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len);

/**
 * Cleanup a connection after a disconnect and free assigned ressources.
 * Always use this when a programm terminates, otherwise it may not start again.
 * @param user_configuration the RaSTA lib configuration
 */
void rasta_cleanup(rasta_lib_configuration_t user_configuration);

#ifdef __cplusplus
}
#endif
