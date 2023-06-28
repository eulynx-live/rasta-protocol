#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include "event_system.h"
#include "rastahandle.h"
#include "rasta_lib.h"

/**
 * size of ring buffer where data is hold for retransmissions
 */
#define MAX_QUEUE_SIZE 100 // TODO: maybe in config file

/**
 * the maximum length of application messages in the data of a RaSTA packet.
 * Length of a SCI PDU is max. 44 bytes
 */
#define MAX_APP_MSG_LEN 60

/**
 * maximum length of a RaSTA packet (16 byte MD4 + 5 * 44 bytes of app messages)
 */
#define MAX_PACKET_LEN 264

#define DIAGNOSTIC_INTERVAL_SIZE 500

typedef struct {
    unsigned long id;
    struct RastaByteArray appMessage;
} rastaApplicationMessage;

void log_main_loop_state(struct rasta_handle *h, event_system *ev_sys, const char *message);

/**
 * initializes the RaSTA handle with a given configuration and logger
 * @param user_configuration the user configuration containing the handle to initialize
 * @param config the configuration to initialize the handle with
 * @param logger the logger to use
 */
void rasta_socket(rasta_lib_configuration_t user_configuration, rasta_config_info *config, struct logger_t *logger);

/**
 * binds a RaSTA instance to the configured IP addresses and ports for the transport channels
 * @param user_configuration the user configuration to be used
 */
void rasta_bind(rasta_lib_configuration_t user_configuration);

/**
 * Listen on all sockets specified by the given RaSTA handle.
 * @param user_configuration the user configuration containing the socket information
*/
void rasta_listen(rasta_lib_configuration_t user_configuration);

/**
 * Wait for connections on all sockets specified in the user_configuration.
 * @param user_configuration the user configuration containing the socket information
*/
struct rasta_connection * rasta_accept(rasta_lib_configuration_t user_configuration);

/**
 * Connect to another rasta instance
 * @param user_configuration the user configuration of the local RaSTA instance
 * @param id the ID of the remote RaSTA instance to connect to
 */
struct rasta_connection* rasta_connect(rasta_lib_configuration_t user_configuration, unsigned long id);

/**
 * Receive data on a given RaSTA connection
 * @param user_configuration the user configuration of the local RaSTA instance
 * @param connection the connection from which to receive the data
 * @param buf the buffer into which to save the received data
 * @param len the size of buf in bytes
*/
int rasta_recv(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len);

/**
 * Send data on a given RaSTA connection
 * @param user_configuration the user configuration of the local RaSTA instance
 * @param connection the connection on which to send the data
 * @param buf the buffer from which to read the data to be sent
 * @param len the size of buf in bytes
*/
int rasta_send(rasta_lib_configuration_t user_configuration, struct rasta_connection *connection, void *buf, size_t len);

/**
 * disconnect a connection on request by the user
 * @param connection the connection that should be disconnected
*/
void rasta_disconnect(struct rasta_connection *connection);

/**
 * Cleanup a connection after a disconnect and free assigned ressources.
 * Always use this when a programm terminates, otherwise it may not start again.
 * @param user_configuration the RaSTA lib configuration
 */
void rasta_cleanup(rasta_lib_configuration_t user_configuration);

#ifdef __cplusplus
}
#endif
