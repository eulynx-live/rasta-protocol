
#include <CUnit/Basic.h>

// INCLUDE TESTS
#include "transport_test.h"

int suite_init(void) {
    return 0;
}

int suite_clean(void) {
    return 0;
}

void cunit_register() {
    CU_pSuite pSuiteMath = CU_add_suite("transport tests", suite_init, suite_clean);

    // Tests for transport_init
    CU_add_test(pSuiteMath, "test_transport_init_should_initialize_channel_props", test_transport_init_should_initialize_channel_props);
    CU_add_test(pSuiteMath, "test_transport_init_should_initialize_receive_event", test_transport_init_should_initialize_receive_event);
    CU_add_test(pSuiteMath, "test_transport_init_should_initialize_receive_event_data", test_transport_init_should_initialize_receive_event_data);
    CU_add_test(pSuiteMath, "test_transport_init_should_add_receive_event_to_event_system", test_transport_init_should_add_receive_event_to_event_system);
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
