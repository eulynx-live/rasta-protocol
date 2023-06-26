#include "transport/transport_test.h"
#include <CUnit/Basic.h>
#include "../../src/c/transport/transport.h"

void test_transport_init_should_initialize_channel_props() {

    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_channel channel;

    // Act
    transport_init(&h, &channel, 100, "localhost", 4711, NULL);

    // Assert

    CU_ASSERT_EQUAL(channel.id, 100);
    CU_ASSERT_EQUAL(channel.remote_port, 4711);
    CU_ASSERT_STRING_EQUAL(channel.remote_ip_address, "localhost");
}

void test_transport_init_should_initialize_receive_event() {

    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_channel channel;

    // Act
    transport_init(&h, &channel, 100, "localhost", 4711, NULL);

    // Assert

    // Receive event should be set up correctly
    CU_ASSERT_EQUAL(channel.receive_event.carry_data, &channel.receive_event_data);
    CU_ASSERT_EQUAL(channel.receive_event.callback, channel_receive_event);
}


void test_transport_init_should_initialize_receive_event_data() {

    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_channel channel;

    // Act
    transport_init(&h, &channel, 100, "localhost", 4711, NULL);

    // Assert

    // Receive event data should be set up correctly
    CU_ASSERT_EQUAL(channel.receive_event_data.h, &h);
    CU_ASSERT_EQUAL(channel.receive_event_data.channel, &channel);
    CU_ASSERT_EQUAL(channel.receive_event_data.connection, NULL);
}

void test_transport_init_should_add_receive_event_to_event_system() {

    // Arrange
    event_system event_system = {0};
    struct rasta_handle h;
    h.ev_sys = &event_system;
    rasta_handle_init(&h, NULL, NULL);

    rasta_transport_channel channel;

    // Act
    transport_init(&h, &channel, 100, "localhost", 4711, NULL);

    // Assert

    CU_ASSERT_EQUAL(event_system.fd_events.first, &channel.receive_event);
}
