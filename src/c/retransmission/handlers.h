#pragma once

#include <rasta/rastahandle.h>
#include <rasta/rastamodule.h>

int handle_discreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_hb(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_data(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_retrreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_retrresp(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
int handle_retrdata(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
struct rasta_connection *handle_conreq(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
struct rasta_connection *handle_conresp(struct rasta_connection *con, struct RastaPacket *receivedPacket);

// protected methods
void update_connection_attrs(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
void update_confirmed_attrs(struct rasta_connection *connection, struct RastaPacket *receivedPacket);
