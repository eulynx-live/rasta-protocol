//
// Created by erica on 04/07/2022.
//
#include "../../src/c/experimental/key_exchange.h"
#include "../../src/c/logging.h"
#include <CUnit/Basic.h>

#ifdef ENABLE_OPAQUE
#include <opaque.h>

void opaque_wrapper_test() {
    const char *psk = "MySecretPW";
    struct key_exchange_state kex_state;
    const uint32_t server_id = 42, client_id = 21, isn = 0xdeadbeef;
    struct logger_t logger;
    logger_init(&logger, LOG_LEVEL_DEBUG, LOGGER_TYPE_CONSOLE);
    uint8_t server_session_key[OPAQUE_SHARED_SECRETBYTES];
    uint8_t server_user_auth[crypto_auth_hmacsha512_BYTES];
    uint8_t client_user_auth[crypto_auth_hmacsha512_BYTES];
    int ret;

    ret = key_exchange_prepare_from_psk(&kex_state, psk, server_id, client_id, &logger);
    CU_ASSERT_EQUAL(ret, 0);

    ret = key_exchange_prepare_credential_request(&kex_state, psk, &logger);
    CU_ASSERT_EQUAL(ret, 0);

    ret = kex_prepare_credential_response(&kex_state, kex_state.client_public, sizeof(kex_state.client_public), server_id, client_id, isn, &logger);
    CU_ASSERT_EQUAL(ret, 0);
    memcpy(server_session_key, kex_state.session_key, OPAQUE_SHARED_SECRETBYTES);
    memcpy(server_user_auth, kex_state.user_auth_server, crypto_auth_hmacsha512_BYTES);

    ret = kex_recover_credential(&kex_state, kex_state.certificate_response, sizeof(kex_state.certificate_response), client_id, server_id, isn, &logger);
    CU_ASSERT_EQUAL(ret, 0);

    // server and client should calculate the same key
    CU_ASSERT_NSTRING_EQUAL(kex_state.session_key, server_session_key, OPAQUE_SHARED_SECRETBYTES);

    memcpy(client_user_auth, kex_state.user_auth_server, crypto_auth_hmacsha512_BYTES);
    memcpy(kex_state.user_auth_server, client_user_auth, crypto_auth_hmacsha512_BYTES);

    ret = kex_authenticate_user(&kex_state, client_user_auth, sizeof(client_user_auth), &logger);
    CU_ASSERT_EQUAL(ret, 0);
}
#endif
