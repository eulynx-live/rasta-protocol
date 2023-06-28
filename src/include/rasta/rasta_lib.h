#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include "logging.h"
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

void init_send_key_exchange_event(timed_event *ev, struct timed_event_data *carry_data,
                                  struct rasta_connection *connection);

void rasta_lib_init_configuration(rasta_lib_configuration_t user_configuration, rasta_config_info *config, struct logger_t *logger, rasta_connection_config *connections, size_t connections_length);

#ifdef __cplusplus
}
#endif
