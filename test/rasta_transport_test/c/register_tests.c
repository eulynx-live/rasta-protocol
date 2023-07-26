
#include <CUnit/Basic.h>

// INCLUDE TESTS
#include "transport_test.h"

#ifdef TEST_TCP
#include "transport_test_tcp.h"
#endif

#ifdef TEST_UDP
#include "transport_test_udp.h"
#endif

#ifdef TEST_TLS
#include "transport_test_tls.h"
#endif

#ifdef TEST_DTLS
#include "transport_test_dtls.h"
#endif

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

    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_socket", test_transport_create_socket_should_initialize_socket);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_create_fd", test_transport_create_socket_should_create_fd);

    #ifdef TEST_TCP
    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event", test_transport_create_socket_should_initialize_accept_event);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event_data", test_transport_create_socket_should_initialize_accept_event_data);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_add_accept_event_to_event_system", test_transport_create_socket_should_add_accept_event_to_event_system);
    #endif

    #ifdef TEST_UDP
    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_receive_event", test_transport_create_socket_should_initialize_receive_event);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_receive_event_data", test_transport_create_socket_should_initialize_receive_event_data);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_add_receive_event_to_event_system", test_transport_create_socket_should_add_receive_event_to_event_system);
    #endif

    #ifdef TEST_TLS
    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event", test_transport_create_socket_should_initialize_accept_event);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event_data", test_transport_create_socket_should_initialize_accept_event_data);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_add_accept_event_to_event_system", test_transport_create_socket_should_add_accept_event_to_event_system);
    #endif

    #ifdef TEST_DTLS
    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event", test_transport_create_socket_should_initialize_accept_event);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event_data", test_transport_create_socket_should_initialize_accept_event_data);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_add_accept_event_to_event_system", test_transport_create_socket_should_add_accept_event_to_event_system);
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
