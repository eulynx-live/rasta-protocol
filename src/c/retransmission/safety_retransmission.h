#pragma once

#include <rasta/event_system.h>
#include <rasta/config.h>
#include <rasta/rastahandle.h>
#include <rasta/logging.h>
#include "messages.h"

/**
 * Copy received messages from a RaSTA packet into the receive buffer.
*/
void sr_add_app_messages_to_buffer(struct rasta_connection *con, struct RastaPacket *packet);

/**
 * removes all confirmed messages from the retransmission fifo
 * @param con the connection that is used
 */
void sr_remove_confirmed_messages(struct rasta_connection *con);

/**
 * Reset connection attributes of @p connection.
*/
void sr_reset_connection(struct rasta_connection *connection);

/**
 * Initialize a @p connection with @p role (client or server) 
*/
void sr_init_connection(struct rasta_connection *connection, rasta_role role);

/**
 * Send a disconnection request on @p connection with reason @p reason
*/
void sr_close_connection(struct rasta_connection *connection, rasta_disconnect_reason reason, unsigned short details);

/**
 * Handle a transport channel closed because of connection failures
*/
void sr_closed_connection(rasta_connection *connection, unsigned long id);

/**
 * Retransmit all messages in the retransmission queue
*/
void sr_retransmit_data(struct rasta_connection *connection);

/**
 * Listen on all sockets specified by the given RaSTA handle.
 * This should not be called from outside the library - use rasta_listen() instead!
 * @param h the RaSTA handle containing the socket information
*/
void sr_listen(struct rasta_handle *h);

/**
 * send data to another instance
 * @param h the handle of the local RaSTA instance
 * @param con the connection to send the data on
 * @param app_messages the messages to send
 */
void sr_send(struct rasta_handle *h, struct rasta_connection *con, struct RastaMessageData app_messages);

/**
 * Handle a received packet on the safety/retransmission level and check validity
 * @param con the connection on which the packet was received
 * @param receivedPacket the packet that was received
*/
int sr_receive(rasta_connection *con, struct RastaPacket *receivedPacket);

/**
 * connects to another rasta instance
 * This should not be called from outside the library - use rasta_connect() instead!
 * @param h the handle of the local RaSTA instance
 * @param id the ID of the remote RaSTA instance to connect to
 */
struct rasta_connection* sr_connect(struct rasta_handle *h, unsigned long id);

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

// validity check functions

/**
 * calculates cts_in_seq for the given @p packet
 * @param con the connection that is used
 * @param packet the packet
 * @return cts_in_seq (bool)
 */
int sr_cts_in_seq(struct rasta_connection *con, rasta_config_sending *cfg, struct RastaPacket *packet);

/**
 * calculates sn_in_seq for the given @p packet
 * @param con the connection that is used
 * @param packet the packet
 * @return sn_in_seq (bool)
 */
int sr_sn_in_seq(struct rasta_connection *con, struct RastaPacket *packet);

/**
 * Checks the sequence number range as in 5.5.3.2
 * @param con connection that is used
 * @param packet the packet
 * @return 1 if the sequency number of the @p packet is in range
 */
int sr_sn_range_valid(struct rasta_connection *con, rasta_config_sending *cfg, struct RastaPacket *packet);

/**
 * checks the confirmed sequence number integrity as in 5.5.4
 * @param con connection that is used
 * @param packet the packet
 * @return 1 if the integrity of the confirmed sequency number is confirmed, 0 otherwise
 */
int sr_cs_valid(struct rasta_connection *con, struct RastaPacket *packet);

/**
 * checks the packet authenticity as in 5.5.2 2)
 * @param con connection that is used
 * @param packet the packet
 * @return 1 if sender and receiver of the @p packet are authentic, 0 otherwise
 */
int sr_message_authentic(struct rasta_connection *con, struct RastaPacket *packet);

/**
 * checks if the received packet is valid
*/
int sr_check_packet(struct rasta_connection *con, struct logger_t *logger, rasta_config_sending *cfg, struct RastaPacket *receivedPacket, char *location);

// Diagnostics
void sr_diagnostic_interval_init(struct rasta_connection *connection, rasta_config_sending *cfg);
void sr_diagnostic_update(struct rasta_connection *connection, struct RastaPacket *receivedPacket, rasta_config_sending *cfg);
void sr_update_timeout_interval(long confirmed_timestamp, struct rasta_connection *con, rasta_config_sending *cfg);

// queue lengths
unsigned int sr_retransmission_queue_item_count(struct rasta_connection *connection);
unsigned int sr_send_queue_item_count(struct rasta_connection *connection);
unsigned int sr_recv_queue_item_count(struct rasta_connection *connection);
