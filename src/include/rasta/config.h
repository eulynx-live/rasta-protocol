#pragma once

#ifdef __cplusplus
extern "C" {  // only need to export C interface if
              // used by C++ source code
#endif

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#include "logging.h"
#include "rastafactory.h"
#include "key_exchange.h"

#define CONFIG_BUFFER_LENGTH 10000

/**
 * defined in 7.2
 */
typedef struct rasta_config_sending {
    unsigned int t_max;
    unsigned int t_h;
    rasta_checksum_type md4_type;
    MD4_u32plus md4_a;
    MD4_u32plus md4_b;
    MD4_u32plus md4_c;
    MD4_u32plus md4_d;
    unsigned short mwa;
    unsigned short send_max;
    unsigned int max_packet;
    unsigned int diag_window;
    unsigned int sr_hash_key;
    rasta_hash_algorithm sr_hash_algorithm;
} rasta_config_sending;

/**
 * Non-standard extension
 */
typedef struct rasta_config_receive {
    unsigned int max_recvqueue_size;
} rasta_config_receive;

/**
 * Non-standard extension
 */
typedef struct rasta_config_retransmission {
    unsigned int max_retransmission_queue_size;
} rasta_config_retransmission;

/**
 * represents an IP and Port
 */
typedef struct rasta_ip_data {
    char ip[16];
    int port;
} rasta_ip_data;

/**
 * represents a list of IP-Port
 */
struct RastaConfigRedundancyConnections {
    rasta_ip_data *data;
    unsigned int count;
};

/**
 * defined in 7.3
 */
typedef struct rasta_config_redundancy {
    struct RastaConfigRedundancyConnections connections;
    struct crc_options crc_type;
    unsigned int t_seq;
    int n_diagnose;
    unsigned int n_deferqueue_size;
} rasta_config_redundancy;

/**
 * defined in 8.1
 */
typedef struct rasta_config_general {
    unsigned long rasta_network;
    unsigned long rasta_id;
} rasta_config_general;

typedef enum rasta_tls_mode
{
    TLS_MODE_DISABLED,
    TLS_MODE_DTLS_1_2,
    TLS_MODE_TLS_1_3
} rasta_tls_mode;

// max length of CN in ASN.1
#define MAX_DOMAIN_LENGTH 64

/**
 * Non-standard extension
 */
typedef struct rasta_config_tls {
    rasta_tls_mode mode;

    /**
     * Path to CA certificate to use, required for server and client operation
     */
    char ca_cert_path[PATH_MAX];
    /**
    * Path to server certificate to use, required for server and client operation
    */
    char cert_path[PATH_MAX];
    /**
    * Path to server private key to use, required for server operation
    */
    char key_path[PATH_MAX];
    /**
     * Domain / common name to validate TLS certificates against (as client)
     */
    char tls_hostname[MAX_DOMAIN_LENGTH];
    /**
     * path to peer certificate for certificate pinning. Optional.
     */
    char peer_tls_cert_path[PATH_MAX];
} rasta_config_tls;


/**
 * stores all presets after load
 */
typedef struct rasta_config_info {
    uint32_t initial_sequence_number;

    size_t accepted_version_count;
    char (*accepted_versions)[5];

    /**
     * all values for the sending part
     */
    rasta_config_sending sending;
    /**
     * all values for the receive part
     */
    rasta_config_receive receive;
    /**
     * all values for the retransmission part
     */
    rasta_config_retransmission retransmission;
    /**
     * all values for the redundancy part
     */
    rasta_config_redundancy redundancy;
    /**
     * includes rastanetwork, receiver and sender id
     * values are 0 if not set in config
     */
    rasta_config_general general;
    /**
     * Configuration for TLS / dTLS setup.
     * Must set mode, and for mode != TLS_MODE_DISABLED, paths to certificate and keys must be set as required
     */
    rasta_config_tls tls;
    /**
     * Configuration for Key Exchange.
     * Must set mode, and for mode != KEX_EXCHANGE_MODE_NONE also psk.
     */
    struct RastaConfigKex kex;
} rasta_config_info;

typedef struct rasta_connection_config {
    /**
     * the RaSTA configuration
     */
    rasta_config_info *config;
    /**
     * the sockets (IP address + port) used by the other connection endpoint 
     */
    rasta_ip_data *transport_sockets;
    size_t transport_sockets_count;
    /**
     * the RaSTA ID of the other connection endpoint
     */
    unsigned long rasta_id;
} rasta_connection_config;

#ifdef __cplusplus
}
#endif
