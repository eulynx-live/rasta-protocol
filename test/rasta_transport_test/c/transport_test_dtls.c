#include "transport_test_dtls.h"
#include <CUnit/Basic.h>
#include "../../src/c/transport/transport.h"

void test_transport_create_socket_should_initialize_accept_event() {
    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_socket socket;
    rasta_config_tls tls_config;

    // Act
    transport_create_socket(&h, &socket, 42, &tls_config);

    // Assert
    CU_ASSERT_PTR_EQUAL(socket.accept_event.callback, channel_accept_event);
    CU_ASSERT_PTR_EQUAL(socket.accept_event.carry_data, &socket.accept_event_data);
    CU_ASSERT_EQUAL(socket.accept_event.fd, socket.file_descriptor);
}

void test_transport_create_socket_should_initialize_accept_event_data() {
    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_socket socket;
    rasta_config_tls tls_config;

    // Act
    transport_create_socket(&h, &socket, 42, &tls_config);

    // Assert
    CU_ASSERT_PTR_EQUAL(socket.accept_event_data.event, &socket.accept_event);
    CU_ASSERT_PTR_EQUAL(socket.accept_event_data.socket, &socket);
    CU_ASSERT_PTR_EQUAL(socket.accept_event_data.h, &h);
}

void test_transport_create_socket_should_add_accept_event_to_event_system() {
    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_socket socket;
    rasta_config_tls tls_config;

    // Act
    transport_create_socket(&h, &socket, 42, &tls_config);

    // Assert
    CU_ASSERT_PTR_EQUAL(event_system.fd_events.last, &socket.accept_event);
}