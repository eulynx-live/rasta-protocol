#pragma once

#ifdef __cplusplus
extern "C" { // only need to export C interface if
             // used by C++ source code
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ENABLE_OPAQUE

#include <opaque.h>

#endif

#define CONFIG_BUFFER_LENGTH 10000

enum KEY_EXCHANGE_MODE {
    KEY_EXCHANGE_MODE_NONE,
#ifdef ENABLE_OPAQUE
    KEY_EXCHANGE_MODE_OPAQUE
#endif
};

#define KEX_PSK_MAX 128

struct RastaConfigKex {
    /**
     * Active Kex mode for the server
     */
    enum KEY_EXCHANGE_MODE mode;
    /**
     * Configured PSK, might be nullptr if mode is KEX_EXCHANGE_MODE_NONE
     */
    char psk[KEX_PSK_MAX];
    /**
     * Rekeying interval or 0 when no rekeying is disabled
     */
    uint64_t rekeying_interval_ms;

#ifdef ENABLE_OPAQUE
    bool has_psk_record;
    uint8_t psk_record[OPAQUE_USER_RECORD_LEN];
#endif
};

/* Any 32-bit or wider unsigned integer data type will do */
typedef unsigned int MD4_u32plus;

/**
 * used checksum type
 */
typedef enum {
    /**
     * no checksum
     */
    RASTA_CHECKSUM_NONE = 0,
    /**
     * 8 byte checksum
     */
    RASTA_CHECKSUM_8B = 1,
    /**
     * 16 byte checksum
     */
    RASTA_CHECKSUM_16B = 2
} rasta_checksum_type;

/**
 * Algorithms that can be used for the RaSTA SR layer checksum
 */
typedef enum {
    /**
     * MD4
     */
    RASTA_ALGO_MD4 = 0,
    /**
     * Blake2b
     */
    RASTA_ALGO_BLAKE2B = 1,
    /**
     * SipHash-2-4
     */
    RASTA_ALGO_SIPHASH_2_4 = 2
} rasta_hash_algorithm;

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
    unsigned int max_recv_msg_size;
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
 * representation of the options the the crc algorithm will use
 */
struct crc_options {
    /**
     * length of crc in bit
     */
    unsigned short width;
    /*
     * the crc polynom without msb
     */
    unsigned long polynom;
    /**
     * the initial value (currently unused)
     */
    unsigned long initial;
    /**
     * the initial value for the table lookup algorithm
     */
    unsigned long initial_optimized;
    /**
     * 0 if reflected input is disabled, 1 otherwise
     */
    int refin;
    /**
     * 0 if reflected output is disabled, 1 otherwise
     */
    int refout;
    /**
     * value for the final xor operation, hast to be the same length as width
     */
    unsigned long final_xor;

    /**
     * mask for internal crc computation, do not set
     */
    unsigned long crc_mask;
    /**
     * mask for internal crc computation, do not set
     */
    unsigned long crc_high_bit;

    /*
     * 1 if the crc lookup table has been generated, 0 otherwise
     */
    int is_table_generated;
    /**
     * the precomputed crc lookup table, generate by calling 'crc_generate_table'
     */
    unsigned long table[256];
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

// max length of CN in ASN.1
#define MAX_DOMAIN_LENGTH 64

/**
 * Non-standard extension
 */
typedef struct rasta_config_tls {
    /**
     * Path to CA certificate to use, required for server and client operation
     */
    char *ca_cert_path;
    /**
     * Path to server certificate to use, required for server and client operation
     */
    char *cert_path;
    /**
     * Path to server private key to use, required for server operation
     */
    char *key_path;
    /**
     * Domain / common name to validate TLS certificates against (as client)
     */
    char tls_hostname[MAX_DOMAIN_LENGTH];
    /**
     * path to peer certificate for certificate pinning. Optional.
     */
    char *peer_tls_cert_path;
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
     * paths to certificate and keys must be set as required
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

/**
 * the log level
 */
typedef enum {
    /**
     * Messages meant for debugging
     */
    LOG_LEVEL_DEBUG = 3,
    /**
     * Messages for general connection status
     */
    LOG_LEVEL_INFO = 2,
    /**
     * Error messages
     */
    LOG_LEVEL_ERROR = 1,
    /**
     * Symbolic log level: nothing will be logged
     * Can only be used as maximum log level
     */
    LOG_LEVEL_NONE = 0
} log_level;

/**
 * represents the type of logging that should be used
 */
typedef enum {
    /**
     * log messages to console
     */
    LOGGER_TYPE_CONSOLE = 0,
    /**
     * log messages to file
     */
    LOGGER_TYPE_FILE = 1,
    /**
     * log to console and file
     */
    LOGGER_TYPE_BOTH = 2
} logger_type;

#ifdef __cplusplus
}
#endif
