#pragma once

#include <rasta/rastahandle.h>
#include <rasta/rastamodule.h>

/**
 * send a Key Exchange Request to the specified host
 * @param connection the connection which should be used
 * @param host the host where the HB will be sent to
 * @param port the port where the HB will be sent to
 */
void send_KexRequest(struct rasta_connection *connection);

int handle_kex_request(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_kex_response(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_kex_auth(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
