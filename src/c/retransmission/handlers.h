#pragma once

#include "../rastahandle.h"
#include "../util/rastamodule.h"

/**
 * processes a received RaSTA packet
 * @param connection the used connection
 * @param receivedPacket the received packet
 */
int handle_received_packet(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received DiscReq packet
 * @param connection the used connection
 * @param receivedPacket the received DiscReq packet
 */
int handle_discreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received Data packet
 * @param connection the used connection
 * @param receivedPacket the received data packet
 */
int handle_data(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received RetrReq packet
 * @param connection the used connection
 * @param receivedPacket the received RetrReq packet
 */
int handle_retrreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received RetrResp packet
 * @param connection the used connection
 * @param receivedPacket the received RetrResp packet
 */
int handle_retrresp(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received RetrData packet
 * @param connection the used connection
 * @param receivedPacket the received RetrData packet
 */
int handle_retrdata(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received Heartbeat packet
 * @param connection the used connection
 * @param receivedPacket the received Heartbeat packet
 */
int handle_hb(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received ConnectionRequest packet
 * @param connection the used connection
 * @param receivedPacket the received ConnectionRequest packet
 */
void handle_conreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

/**
 * processes a received ConnectionResponse packet
 * @param connection the used connection
 * @param receivedPacket the received ConnectionResponse packet
 */
void handle_conresp(struct rasta_connection *connection, struct RastaPacket *receivedPacket);

// protected methods
void update_connection_attrs(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
void update_confirmed_attrs(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
