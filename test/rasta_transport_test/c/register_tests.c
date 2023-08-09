
#include <CUnit/Basic.h>

// INCLUDE TESTS
#include "transport_test.h"

#ifdef TEST_TCP
// TCP and TLS tests
#include "transport_test_tcp.h"
#endif

#ifdef TEST_UDP
// UDP and DTLS tests
#include "transport_test_udp.h"
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

    // Tests for transport_connect
    CU_add_test(pSuiteMath, "test_transport_connect_should_set_connected", test_transport_connect_should_set_connected);
    CU_add_test(pSuiteMath, "test_transport_connect_should_set_equal_fds", test_transport_connect_should_set_equal_fds);

#ifdef TEST_TCP
    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event", test_transport_create_socket_should_initialize_accept_event);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_accept_event_data", test_transport_create_socket_should_initialize_accept_event_data);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_add_accept_event_to_event_system", test_transport_create_socket_should_add_accept_event_to_event_system);

    // Tests for transport_listen
    CU_add_test(pSuiteMath, "test_transport_listen_should_enable_socket_accept_event", test_transport_listen_should_enable_socket_accept_event);

    // Tests for transport_connect
    CU_add_test(pSuiteMath, "test_transport_connect_should_enable_channel_receive_event", test_transport_connect_should_enable_channel_receive_event);

    // Tests for transport_close_channel
    CU_add_test(pSuiteMath, "test_transport_close_channel_should_set_unconnected", test_transport_close_channel_should_set_unconnected);
    CU_add_test(pSuiteMath, "test_transport_close_channel_should_invalidate_fd", test_transport_close_channel_should_invalidate_fd);
    CU_add_test(pSuiteMath, "test_transport_close_channel_should_disable_channel_receive_event", test_transport_close_channel_should_disable_channel_receive_event);

    // Tests for transport_redial
    CU_add_test(pSuiteMath, "test_transport_redial_should_reconnect", test_transport_redial_should_reconnect);
    CU_add_test(pSuiteMath, "test_transport_redial_should_assign_new_fds", test_transport_redial_should_assign_new_fds);
    CU_add_test(pSuiteMath, "test_transport_redial_should_update_event_fds", test_transport_redial_should_update_event_fds);
#endif

#ifdef TEST_UDP
    // Tests for transport_create_socket
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_receive_event", test_transport_create_socket_should_initialize_receive_event);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_initialize_receive_event_data", test_transport_create_socket_should_initialize_receive_event_data);
    CU_add_test(pSuiteMath, "test_transport_create_socket_should_add_receive_event_to_event_system", test_transport_create_socket_should_add_receive_event_to_event_system);

    // Tests for transport_listen
    CU_add_test(pSuiteMath, "test_transport_listen_should_enable_socket_receive_event", test_transport_listen_should_enable_socket_receive_event);

    // Tests for transport_connect
    CU_add_test(pSuiteMath, "test_transport_connect_should_enable_socket_receive_event", test_transport_connect_should_enable_socket_receive_event);
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
