#pragma once

#include <rasta/config.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef ENABLE_OPAQUE

#include <opaque.h>

#endif

#include "../logging.h"

/**
 * Allow client's rekeying Key Exchange Request to be received up to 500 ms after it was due
 */
#define REKEYING_ALLOWED_DELAY_MS 500
/**
 * Holds state required for key exchange
 */
struct key_exchange_state {
#ifdef ENABLE_OPAQUE
    /**
     * User "registration" record, based on the PSK and the IDs
     */
    uint8_t user_record[OPAQUE_USER_RECORD_LEN];
    /**
     * Holds the PSK and other secret data in the client
     */
    uint8_t *client_secret;
    /**
     * Holds the client public record to be sent to or received by the server
     */
    uint8_t client_public[OPAQUE_USER_SESSION_PUBLIC_LEN];
    /**
     * Certificate response from server
     */
    uint8_t certificate_response[OPAQUE_SERVER_SESSION_LEN];
    /**
     * Common secret session key
     */
    uint8_t session_key[OPAQUE_SHARED_SECRETBYTES];
    /**
     * Data required by server to explicitly authenticate client
     */
    uint8_t user_auth_server[crypto_auth_hmacsha512_BYTES];

    size_t password_length;

    /**
     * Timestamp when last key exchange happened
     */
    uint64_t last_key_exchanged_millis;
#endif
    enum KEY_EXCHANGE_MODE active_mode;
};

#define CONFIGURATION_FILE_USER_RECORD_HEADER "URV1"

/**
 * [SERVER] Prepare a user record from a PSK and the RaSTA IDs
 * @param kex_state Key exchange state
 * @param psk Null-terminated pre-shared key
 * @param my_id server RaSTA ID
 * @param remote_id client RaSTA ID
 * @param logger
 * @return 0 on success
 */
int key_exchange_prepare_from_psk(struct key_exchange_state *kex_state, const char *psk, uint32_t my_id,
                                  uint32_t remote_id, struct logger_t *logger);

/**
 * [CLIENT] Prepare an opaque client request to be sent to the server.
 * @param kex_state Key exchange state
 * @param psk Null-terminated pre-shared key
 * @param logger
 * @return 0 on success
 */
int key_exchange_prepare_credential_request(struct key_exchange_state *kex_state, const char *psk,
                                            struct logger_t *logger);

/**
 * [SERVER] process client connection request and prepare connection response in kex_state
 * @param kex_state Key exchange state
 * @param received_client_public received credential request
 * @param received_client_public_length number of bytes in received request
 * @param my_id server RaSTA ID
 * @param remote_id client RaSTA ID
 * @param initial_sequence_number server ISN
 * @param logger
 * @return 0 on success
 */
int kex_prepare_credential_response(struct key_exchange_state *kex_state,
                                    const uint8_t *received_client_public,
                                    size_t received_client_public_length,
                                    uint32_t my_id, uint32_t remote_id,
                                    uint32_t initial_sequence_number, struct logger_t *logger);

/**
 * [CLIENT] recover credential from key exchange
 * @param kex_state Key exchange state
 * @param received_server_response received credential response
 * @param received_server_response_len the length of @p received_server_response in bytes
 * @param my_id client RaSTA ID
 * @param remote_id server RaSTA ID
 * @param initial_sequence_number server ISN
 * @param logger
 * @return 0 on success
 */
int kex_recover_credential(struct key_exchange_state *kex_state,
                           const uint8_t *received_server_response,
                           size_t received_server_response_len,
                           uint32_t my_id, uint32_t remote_id,
                           uint32_t initial_sequence_number, struct logger_t *logger);

/**
 * [SERVER] Compares authentication token received from client with authentication token received from server
 * @param kex_state Kex exchange state
 * @param received_user_auth key exchange received from client
 * @param received_user_auth_length number of bytes in key exchange response
 * @param logger
 * @return 0 on success
 */
int kex_authenticate_user(const struct key_exchange_state *kex_state, const uint8_t *received_user_auth,
                          size_t received_user_auth_length, struct logger_t *logger);
