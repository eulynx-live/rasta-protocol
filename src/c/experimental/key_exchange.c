#include <errno.h>
#include <sys/mman.h>

#include "../logging.h"
#include "../rastahandle.h"

#ifdef ENABLE_OPAQUE
#include "key_exchange.h"
static inline Opaque_Ids rasta_ids_to_opaque_ids(const uint32_t *my_id,
                                                 const uint32_t *remote_id) { // make sure IDs are represented the same way regardless of endianness
    const Opaque_Ids ids = {.idS = (uint8_t *)my_id, .idS_len = sizeof(uint32_t), .idU = (uint8_t *)remote_id, .idU_len = sizeof(uint32_t)};
    return ids;
}

int key_exchange_prepare_from_psk(struct key_exchange_state *kex_state, const char *psk, const uint32_t my_id,
                                  const uint32_t remote_id, struct logger_t *logger) {
    const size_t password_length = strlen(psk);
    const uint32_t my_id_be = htobe32(my_id), remote_id_be = htobe32(remote_id);
    Opaque_Ids ids = rasta_ids_to_opaque_ids(&my_id_be, &remote_id_be);

    (void)logger;
    return opaque_Register((const uint8_t *)psk, password_length, NULL, &ids, kex_state->user_record, NULL);
}

int key_exchange_prepare_credential_request(struct key_exchange_state *kex_state, const char *psk,
                                            struct logger_t *logger) {
    const size_t password_length = strlen(psk);
    int ret;
    uint8_t *client_secret = malloc(password_length + OPAQUE_USER_SESSION_SECRET_LEN);
    if (!client_secret) {
        logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:key_exchange_prepare_credential_request",
                   "Could not allocate %lu bytes for client secret!", password_length + OPAQUE_USER_SESSION_SECRET_LEN);
        return 1;
    }
    // Work around GCC warning which complains about passing the (const) pointer to mlock although
    // the data behind it is not yet initialized. We treat this warning as an error.
    client_secret[0] = 0;
    // make sure the data cannot be stolen from swap partition
    ret = mlock(client_secret, password_length + OPAQUE_USER_SESSION_SECRET_LEN);
    if (ret) {
        logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:key_exchange_prepare_credential_request",
                   "Could not lock client pages!");
        return ret;
    }
    kex_state->client_secret = client_secret;
    kex_state->password_length = password_length;

    return opaque_CreateCredentialRequest((const uint8_t *)psk, password_length, kex_state->client_secret,
                                          kex_state->client_public);
}

int kex_prepare_credential_response(struct key_exchange_state *kex_state,
                                    const uint8_t *received_client_public,
                                    const size_t received_client_public_length,
                                    const uint32_t my_id, const uint32_t remote_id,
                                    const uint32_t initial_sequence_number, struct logger_t *logger) {
    const uint32_t my_id_be = htobe32(my_id), remote_id_be = htobe32(remote_id);
    Opaque_Ids ids = rasta_ids_to_opaque_ids(&my_id_be, &remote_id_be);
    const uint32_t isn_be = htobe32(initial_sequence_number);

    if (received_client_public_length != sizeof(kex_state->client_public)) {
        logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:kex_prepare_credential_response", "Preparing credential response failed: unexpected length of credential request: %lu", received_client_public_length);
        return -1;
    }
    // we use the (random) ISN of the server as connection context here, as we can guarantee that it is different for each new connection
    return opaque_CreateCredentialResponse(received_client_public, kex_state->user_record, &ids,
                                           (const uint8_t *)&isn_be, sizeof(uint32_t),
                                           kex_state->certificate_response, kex_state->session_key,
                                           kex_state->user_auth_server);
}

int kex_recover_credential(struct key_exchange_state *kex_state,
                           const uint8_t *received_server_response,
                           const size_t received_server_response_len,
                           const uint32_t my_id, const uint32_t remote_id,
                           const uint32_t initial_sequence_number, struct logger_t *logger) {
    (void)logger;
    const uint32_t isn_be = htobe32(initial_sequence_number);
    const uint32_t my_id_be = htobe32(my_id), remote_id_be = htobe32(remote_id);
    // identities are swapped from the PoV of the client
    Opaque_Ids ids = rasta_ids_to_opaque_ids(&remote_id_be, &my_id_be);
    int ret, munlock_ret;

    if (received_server_response_len != sizeof(kex_state->certificate_response)) {
        logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:kex_recover_credential", "Recovering credentials failed: unexpected length of credential response: %lu", received_server_response_len);
        return -1;
    }

    // we use the (random) ISN of the server as connection context here, as we can guarantee that it is different for each new connection
    ret = opaque_RecoverCredentials(received_server_response, kex_state->client_secret, (const uint8_t *)&isn_be, sizeof(uint32_t), &ids,
                                    kex_state->session_key, kex_state->user_auth_server,
                                    NULL);
    if (ret) {
        logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:kex_recover_credential",
                   "Recovering credentials failed: %d!", ret);
    }
    // make sure the secret is destroyed properly
    memset(kex_state->client_secret, 0, kex_state->password_length);
    munlock_ret = munlock(kex_state->client_secret, kex_state->password_length);
    if (munlock_ret) {
        logger_log(logger, LOG_LEVEL_ERROR, "kex_exchange:kex_recover_credential", "munlock failed: %s", strerror(errno));
    }
    free(kex_state->client_secret);
    kex_state->client_secret = NULL;

    return ret;
}

int kex_authenticate_user(const struct key_exchange_state *kex_state, const uint8_t *received_user_auth, const size_t received_user_auth_length, struct logger_t *logger) {
    (void)logger;
    if (received_user_auth_length != sizeof(kex_state->user_auth_server)) {
        logger_log(logger, LOG_LEVEL_ERROR, "kex_exchange:kex_authenticate_user", "User authentication packet has unexpected length: %lu", received_user_auth_length);
        return 1;
    }
    return opaque_UserAuth(kex_state->user_auth_server, received_user_auth);
}
#else
int key_exchange_prepare_from_psk(struct key_exchange_state *kex_state, const char *psk, const uint32_t my_id,
                                  const uint32_t remote_id, struct logger_t *logger) {
    (void)kex_state;
    (void)psk;
    (void)my_id;
    (void)remote_id;
    logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:key_exchange_prepare_from_psk",
               "Error: not implemented!");
    return 1;
}

int key_exchange_prepare_credential_request(struct key_exchange_state *kex_state, const char *psk,
                                            struct logger_t *logger) {
    (void)kex_state;
    (void)psk;
    logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:key_exchange_prepare_credential_request",
               "Error: not implemented!");
    return 1;
}

int kex_prepare_credential_response(struct key_exchange_state *kex_state,
                                    const uint8_t *received_client_public,
                                    const size_t length,
                                    const uint32_t my_id, const uint32_t remote_id,
                                    const uint32_t initial_sequence_number, struct logger_t *logger) {
    (void)kex_state;
    (void)received_client_public;
    (void)length;
    (void)my_id;
    (void)remote_id;
    (void)initial_sequence_number;
    logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:kex_prepare_credential_response",
               "Error: not implemented!");
    return 1;
}

int kex_recover_credential(struct key_exchange_state *kex_state,
                           const uint8_t *received_server_response,
                           const size_t length,
                           const uint32_t my_id, const uint32_t remote_id,
                           const uint32_t initial_sequence_number, struct logger_t *logger) {
    (void)kex_state;
    (void)received_server_response;
    (void)length,
        (void)my_id;
    (void)remote_id;
    (void)initial_sequence_number;
    logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:kex_recover_credential",
               "Error: not implemented!");
    return 1;
}

int kex_authenticate_user(const struct key_exchange_state *kex_state, const uint8_t *received_user_auth, const size_t received_user_auth_length, struct logger_t *logger) {
    (void)kex_state;
    (void)received_user_auth;
    (void)received_user_auth_length;
    logger_log(logger, LOG_LEVEL_ERROR, "key_exchange:kex_authenticate_user",
               "Error: not implemented!");
    return 1;
}
#endif
