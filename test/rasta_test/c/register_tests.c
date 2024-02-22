
#include <CUnit/Basic.h>

// INCLUDE TESTS
#include "blake2_test.h"
#include "config_test.h"
#include "dictionary_test.h"
#include "fifo_test.h"
#include "opaque_test.h"
#include "rastacrc_test.h"
#include "rastadeferqueue_test.h"
#include "rastafactory_test.h"
#include "rastamd4_test.h"
#include "rastamodule_test.h"
#include "redundancy_channel_test.h"
#include "safety_retransmission_test.h"

int suite_init(void) {
    return 0;
}

int suite_clean(void) {
    return 0;
}

void cunit_register() {
    CU_pSuite pSuiteRasta = CU_add_suite("rasta tests", suite_init, suite_clean);
    CU_add_test(pSuiteRasta, "testConversion", testConversion);

    // MD4 tests
    CU_add_test(pSuiteRasta, "testMD4function", testMD4function);
    CU_add_test(pSuiteRasta, "testRastaMD4Sample", testRastaMD4Sample);

    // Tests for the crc module
    CU_add_test(pSuiteRasta, "test_opt_b", test_opt_b);
    CU_add_test(pSuiteRasta, "test_opt_c", test_opt_c);
    CU_add_test(pSuiteRasta, "test_opt_d", test_opt_d);
    CU_add_test(pSuiteRasta, "test_opt_e", test_opt_e);
    CU_add_test(pSuiteRasta, "test_without_gen_table", test_without_gen_table);

    // Tests for rastafactory
    CU_add_test(pSuiteRasta, "checkConnectionPacket", checkConnectionPacket);
    CU_add_test(pSuiteRasta, "checkNormalPacket", checkNormalPacket);
    CU_add_test(pSuiteRasta, "checkDisconnectionRequest", checkDisconnectionRequest);
    CU_add_test(pSuiteRasta, "checkMessagePacket", checkMessagePacket);

    // Tests for the Redundancy layer factory and model
    CU_add_test(pSuiteRasta, "testRedundancyConversionWithCrcChecksumCorrect", testRedundancyConversionWithCrcChecksumCorrect);
    CU_add_test(pSuiteRasta, "testRedundancyConversionWithoutChecksum", testRedundancyConversionWithoutChecksum);
    CU_add_test(pSuiteRasta, "testRedundancyConversionIncorrectChecksum", testRedundancyConversionIncorrectChecksum);
    CU_add_test(pSuiteRasta, "testCreateRedundancyPacket", testCreateRedundancyPacket);
    CU_add_test(pSuiteRasta, "testCreateRedundancyPacketNoChecksum", testCreateRedundancyPacketNoChecksum);

    // Test for dictionary
    CU_add_test(pSuiteRasta, "testDictionary", testDictionary);

    // Test for config
    CU_add_test(pSuiteRasta, "check_std_config", check_std_config);
    CU_add_test(pSuiteRasta, "check_var_config", check_var_config);

    // Tests for the defer queue
    CU_add_test(pSuiteRasta, "test_deferqueue_init", test_deferqueue_init);
    CU_add_test(pSuiteRasta, "test_deferqueue_add", test_deferqueue_add);
    CU_add_test(pSuiteRasta, "test_deferqueue_add_full", test_deferqueue_add_full);
    CU_add_test(pSuiteRasta, "test_deferqueue_remove", test_deferqueue_remove);
    CU_add_test(pSuiteRasta, "test_deferqueue_remove_not_in_queue", test_deferqueue_remove_not_in_queue);
    CU_add_test(pSuiteRasta, "test_deferqueue_contains", test_deferqueue_contains);
    CU_add_test(pSuiteRasta, "test_deferqueue_smallestseqnr", test_deferqueue_smallestseqnr);
    CU_add_test(pSuiteRasta, "test_deferqueue_destroy", test_deferqueue_destroy);
    CU_add_test(pSuiteRasta, "test_deferqueue_isfull", test_deferqueue_isfull);
    CU_add_test(pSuiteRasta, "test_deferqueue_get", test_deferqueue_get);
    CU_add_test(pSuiteRasta, "test_deferqueue_sorted", test_deferqueue_sorted);
    CU_add_test(pSuiteRasta, "test_deferqueue_get_ts", test_deferqueue_get_ts);
    CU_add_test(pSuiteRasta, "test_deferqueue_clear", test_deferqueue_clear);
    CU_add_test(pSuiteRasta, "test_deferqueue_get_ts_doesnt_contain", test_deferqueue_get_ts_doesnt_contain);

    // Tests for the FIFO
    CU_add_test(pSuiteRasta, "test_push", test_push);
    CU_add_test(pSuiteRasta, "test_pop", test_pop);

    // Tests for BLAKE2 hashes
    CU_add_test(pSuiteRasta, "testBlake2Hash", testBlake2Hash);

    // Tests for Safety and Retransmission layer
    CU_add_test(pSuiteRasta, "test_sr_retransmit_data_shouldSendFinalHeartbeat", test_sr_retransmit_data_shouldSendFinalHeartbeat);
    CU_add_test(pSuiteRasta, "test_sr_retransmit_data_shouldRetransmitPackage", test_sr_retransmit_data_shouldRetransmitPackage);

    CU_add_test(pSuiteRasta, "test_redundancy_channel", test_redundancy_channel);

    // Tests for OPAQUE
#ifdef ENABLE_OPAQUE
    CU_add_test(pSuiteRasta, "opaque_wrapper_test", opaque_wrapper_test);
#endif
}

int main() {
    if (CUE_SUCCESS != CU_initialize_registry())
        return CU_get_error();

    cunit_register();

    CU_basic_run_tests();
    int ret = CU_get_number_of_failures();
    CU_cleanup_registry();
    return ret;
}
