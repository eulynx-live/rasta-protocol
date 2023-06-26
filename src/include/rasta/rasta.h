#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

// TODO: check
//#include <errno.h>
#include "event_system.h"
#include "rastahandle.h"

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

/**
 * initializes the rasta handle and starts all threads
 * configuration is loaded from file
 * @param handle
 * @param config_file_path
 * @param listenports
 * @param port_count
 */
void rasta_socket(struct rasta_handle *handle, rasta_config_info *config, struct logger_t *logger);

void rasta_bind(struct rasta_handle *handle);

void sr_listen(struct rasta_handle *h);

/**
 * connects to another rasta instance
 * @param handle
 * @param id
 */
struct rasta_connection* sr_connect(struct rasta_handle *h, unsigned long id);

/**
 * send data to another instance
 * @param h
 * @param remote_id
 * @param app_messages
 */
void sr_send(struct rasta_handle *h, struct rasta_connection *con, struct RastaMessageData app_messages);

/**
 * get data from message buffer
 * this is used in the onReceive Event to get the received message
 * @param h
 * @param connection
 * @return the applicationmessage, where id is the sender rasta id and appMessage is the received data
 */
rastaApplicationMessage sr_get_received_data(struct rasta_handle *h, struct rasta_connection *connection);

/**
 * disconnect a connection on request by the user
 * @param con the connection that should be disconnected
*/
void rasta_disconnect(struct rasta_connection *connection);

void sr_recv(struct rasta_handle *h, event_system *event_system, int wait_for_handshake, int listen);

// Event handlers
int event_connection_expired(void *carry_data);
int heartbeat_send_event(void *carry_data);
int send_timed_key_exchange(void *arg);

#ifdef __cplusplus
}
#endif
