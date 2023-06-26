#pragma once

#include <rasta/event_system.h>
#include <rasta/config.h>
#include <rasta/rastahandle.h>
#include <rasta/logging.h>
#include "messages.h"

void updateTimeoutInterval(long confirmed_timestamp, struct rasta_connection *con, rasta_config_sending *cfg);
void updateDiagnostic(struct rasta_connection *connection, struct RastaPacket *receivedPacket, rasta_config_sending *cfg);
void sr_add_app_messages_to_buffer(struct rasta_connection *con, struct RastaPacket *packet);
void sr_remove_confirmed_messages(struct rasta_connection *con);
void sr_reset_connection(struct rasta_connection *connection);
void sr_close_connection(struct rasta_connection *connection, rasta_disconnect_reason reason, unsigned short details);
void sr_diagnostic_interval_init(struct rasta_connection *connection, rasta_config_sending *cfg);
void sr_init_connection(struct rasta_connection *connection, rasta_role role);
void sr_retransmit_data(struct rasta_connection *connection);
void rasta_socket(struct rasta_handle *handle, rasta_config_info *config, struct logger_t *logger);

/**
 * Listen on all sockets specified by the given RaSTA handle.
 * This should not be called from outside the library - use rasta_listen() instead!
 * @param h the RaSTA handle containing the socket information
*/
void sr_listen(struct rasta_handle *h);

/**
 * Disconnect a connection on request by the user.
 * This should not be called from outside the library - use rasta_disconnect() instead!
 * @param con the connection that should be disconnected
*/
void sr_disconnect(struct rasta_connection *con);


/**
 * Cleanup a connection after a disconnect and free assigned ressources.
 * Always use this when a programm terminates, otherwise it may not start again.
 * This should not be called from outside the library - use rasta_cleanup() instead!
 * @param h the handle of the RaSTA instance
 */
void sr_cleanup(struct rasta_handle *h);

int sr_cts_in_seq(struct rasta_connection *con, rasta_config_sending *cfg, struct RastaPacket *packet);
int sr_sn_in_seq(struct rasta_connection *con, struct RastaPacket *packet);
int sr_sn_range_valid(struct rasta_connection *con, rasta_config_sending *cfg, struct RastaPacket *packet);
int sr_cs_valid(struct rasta_connection *con, struct RastaPacket *packet);
int sr_message_authentic(struct rasta_connection *con, struct RastaPacket *packet);
int sr_check_packet(struct rasta_connection *con, struct logger_t *logger, rasta_config_sending *cfg, struct RastaPacket *receivedPacket, char *location);
unsigned int sr_retransmission_queue_item_count(struct rasta_connection *connection);
unsigned int sr_send_queue_item_count(struct rasta_connection *connection);
int sr_receive(rasta_connection *con, struct RastaPacket *receivedPacket);
void sr_closed_connection(rasta_connection *connection, unsigned long id);
void sr_set_receive_buffer(void *buf, size_t len);
size_t sr_get_received_data_len();
