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
 * closes the connection to the connection
 * @param h
 * @param con
 */
void sr_disconnect(struct rasta_connection *con);

/**
 * used to end all threads an free assigned ressources
 * always use this when a programm terminates otherwise it may not start again
 * @param h
 */
void sr_cleanup(struct rasta_handle *h);

// Event handlers
int event_connection_expired(void *carry_data);
int heartbeat_send_event(void *carry_data);
int send_timed_key_exchange(void *arg);

#ifdef __cplusplus
}
#endif
