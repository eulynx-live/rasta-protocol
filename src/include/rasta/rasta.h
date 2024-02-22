#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <stdbool.h>
#include <stddef.h>

#include "config.h"
#include "events.h"
#include "notification.h"
#include "rastarole.h"

typedef struct rasta rasta;
typedef struct rasta_connection rasta_connection;
typedef struct rasta_cancellation rasta_cancellation;

/**
 * initializes the RaSTA handle and all configured connections
 * @param rasta the user configuration containing the handle to initialize
 * @param config the configuration to initialize the handle with
 * @param logger the logger to use
 */
rasta *rasta_lib_init_configuration(rasta_config_info *config, log_level log_level, logger_type logger_type);

/**
 * binds a RaSTA instance to the configured IP addresses and ports for the transport channels
 * @param rasta the user configuration to be used
 */
bool rasta_bind(rasta *r);

/**
 * Listen on all sockets specified by the given RaSTA handle.
 * @param rasta the user configuration containing the socket information
 */
void rasta_listen(rasta *r);

/**
 * Wait for incoming connections.
 * @param rasta the user configuration containing the socket information
 */
rasta_connection *rasta_accept(rasta *r);

/**
 * Prepares cancellation of a blocking operation.
 */
rasta_cancellation *rasta_prepare_cancellation(rasta *r);

/**
 * Wait for incoming connections with the ability to cancel from another thread.
 * @param rasta the user configuration containing the socket information
 */
rasta_connection *rasta_accept_with_cancel(rasta *r, rasta_cancellation *cancel);

/**
 * Performs cancellation of a blocking operation.
 */
void rasta_cancel_operation(rasta *r, rasta_cancellation *cancel);

/**
 * Connect to another rasta instance
 * @param rasta the user configuration of the local RaSTA instance
 * @param id the ID of the remote RaSTA instance to connect to
 */
rasta_connection *rasta_connect(rasta *r);

/**
 * Receive data on a given RaSTA connection
 * @param rasta the user configuration of the local RaSTA instance
 * @param connection the connection from which to receive the data
 * @param buf the buffer into which to save the received data
 * @param len the size of buf in bytes
 */
int rasta_recv(rasta *r, rasta_connection *connection, void *buf, size_t len);

/**
 * Send data on a given RaSTA connection
 * @param rasta the user configuration of the local RaSTA instance
 * @param connection the connection on which to send the data
 * @param buf the buffer from which to read the data to be sent
 * @param len the size of buf in bytes
 */
int rasta_send(rasta *r, rasta_connection *connection, void *buf, size_t len);

/**
 * disconnect a connection on request by the user
 * @param connection the connection that should be disconnected
 */
void rasta_disconnect(rasta_connection *connection);

/**
 * Cleanup a connection after a disconnect and free assigned ressources.
 * Always use this when a programm terminates, otherwise it may not start again.
 * @param rasta the RaSTA lib configuration
 */
void rasta_cleanup(rasta *r);

#ifdef __cplusplus
}
#endif
