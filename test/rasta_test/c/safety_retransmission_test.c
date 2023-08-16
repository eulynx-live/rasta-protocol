#include "../headers/safety_retransmission_test.h"
#include <CUnit/Basic.h>
#include <rasta/rastahandle.h>
#include <rasta/rastaredundancy.h>
#include <rasta/rastautil.h>
#include <rasta/rmemory.h>

#include "../../../src/c/retransmission/safety_retransmission.h"
#include "../../../src/c/transport/transport.h"

#define SERVER_ID 0xA

static fifo_t *test_send_fifo = NULL;

void fake_send_callback(struct RastaByteArray data_to_send, rasta_transport_channel *channel) {
    if (test_send_fifo == NULL) {
        test_send_fifo = fifo_init(128);
    }

    struct RastaByteArray *to_fifo = rmalloc(sizeof(struct RastaByteArray));
    allocateRastaByteArray(to_fifo, data_to_send.length);
    rmemcpy(to_fifo->bytes, data_to_send.bytes, data_to_send.length);

    fifo_push(test_send_fifo, to_fifo);
}

void test_sr_retransmit_data_shouldSendFinalHeartbeat() {
    fifo_destroy(&test_send_fifo);

    rasta_receive_handle h;
    struct logger_t logger = logger_init(LOG_LEVEL_INFO, LOGGER_TYPE_CONSOLE);
    h.logger = &logger;

    rasta_config_info info;
    rasta_config_redundancy configInfoRedundancy;
    configInfoRedundancy.t_seq = 100;
    configInfoRedundancy.n_diagnose = 10;
    configInfoRedundancy.crc_type = crc_init_opt_a();
    configInfoRedundancy.n_deferqueue_size = 2;
    info.redundancy = configInfoRedundancy;

    rasta_config_retransmission configRetransmission;
    configRetransmission.max_retransmission_queue_size = 100;
    info.retransmission = configRetransmission;

    uint16_t listenPorts[1] = {1234};
    redundancy_mux mux = redundancy_mux_init(&logger, listenPorts, 1, &info);
    mux.sr_hashing_context.hash_length = RASTA_CHECKSUM_NONE;
    rasta_md4_set_key(&mux.sr_hashing_context, 0, 0, 0, 0);

    rasta_redundancy_channel fake_channel;
    fake_channel.mux = &mux;
    fake_channel.associated_id = SERVER_ID;
    fake_channel.hashing_context.algorithm = RASTA_ALGO_MD4;
    fake_channel.hashing_context.hash_length = RASTA_CHECKSUM_NONE;
    fake_channel.seq_tx = 0;
    rasta_md4_set_key(&fake_channel.hashing_context, 0, 0, 0, 0);

    rasta_transport_channel transport;
    transport.send_callback = fake_send_callback;
    transport.connected = true;
    transport.remote_port = 1234;
    strncpy(transport.remote_ip_address, "127.0.0.1", 10);

    fake_channel.transport_channels = &transport;
    fake_channel.transport_channel_count = 1;

    mux.redundancy_channels = &fake_channel;
    mux.redundancy_channels_count = 1;

    rasta_connection connection;
    connection.remote_id = SERVER_ID;
    connection.fifo_retransmission = fifo_init(0);
    connection.redundancy_channel = &fake_channel;
    connection.config = &info;
    connection.logger = &logger;

    sr_retransmit_data(&connection);

    // One message should be sent
    CU_ASSERT_PTR_NOT_NULL_FATAL(test_send_fifo);
    CU_ASSERT_EQUAL(1, fifo_get_size(test_send_fifo));

    struct RastaByteArray *hb_message = fifo_pop(test_send_fifo);
    CU_ASSERT_EQUAL(36, hb_message->length);
    // 8 bytes retransmission header, 2 bytes offset for message type
    CU_ASSERT_EQUAL(RASTA_TYPE_HB, leShortToHost(hb_message->bytes + 8 + 2));

    fifo_destroy(&connection.fifo_retransmission);

    freeRastaByteArray(hb_message);
    rfree(hb_message);

    rfree(mux.transport_sockets);
    freeRastaByteArray(&fake_channel.hashing_context.key);
    freeRastaByteArray(&mux.sr_hashing_context.key);
}

void test_sr_retransmit_data_shouldRetransmitPackage() {
    fifo_destroy(&test_send_fifo);

    // Arrange
    rasta_receive_handle h;
    struct logger_t logger = logger_init(LOG_LEVEL_INFO, LOGGER_TYPE_CONSOLE);
    h.logger = &logger;

    rasta_config_info info;
    rasta_config_redundancy configInfoRedundancy;
    configInfoRedundancy.t_seq = 100;
    configInfoRedundancy.n_diagnose = 10;
    configInfoRedundancy.crc_type = crc_init_opt_a();
    configInfoRedundancy.n_deferqueue_size = 2;
    info.redundancy = configInfoRedundancy;

    rasta_config_retransmission configRetransmission;
    configRetransmission.max_retransmission_queue_size = 100;
    info.retransmission = configRetransmission;

    uint16_t listenPorts[1] = {1234};
    redundancy_mux mux = redundancy_mux_init(&logger, listenPorts, 1, &info);
    mux.sr_hashing_context.hash_length = RASTA_CHECKSUM_NONE;
    rasta_md4_set_key(&mux.sr_hashing_context, 0, 0, 0, 0);

    rasta_redundancy_channel fake_channel;
    fake_channel.mux = &mux;
    fake_channel.associated_id = SERVER_ID;
    fake_channel.hashing_context.algorithm = RASTA_ALGO_MD4;
    fake_channel.hashing_context.hash_length = RASTA_CHECKSUM_NONE;
    fake_channel.seq_tx = 0;
    rasta_md4_set_key(&fake_channel.hashing_context, 0, 0, 0, 0);

    rasta_transport_channel transport;
    transport.send_callback = fake_send_callback;
    transport.connected = true;
    transport.remote_port = 1234;
    strncpy(transport.remote_ip_address, "127.0.0.1", 10);

    fake_channel.transport_channels = &transport;
    fake_channel.transport_channel_count = 1;

    mux.redundancy_channels = &fake_channel;
    mux.redundancy_channels_count = 1;

    struct rasta_connection connection;
    connection.remote_id = SERVER_ID;
    connection.fifo_retransmission = fifo_init(1);
    connection.redundancy_channel = &fake_channel;
    connection.config = &info;
    connection.logger = &logger;

    struct RastaMessageData app_messages;
    struct RastaByteArray message;
    message.bytes = "Hello world";
    message.length = strlen(message.bytes) + 1;
    app_messages.count = 1;
    app_messages.data_array = &message;

    rasta_hashing_context_t hashing_context;
    hashing_context.algorithm = RASTA_ALGO_MD4;
    hashing_context.hash_length = RASTA_CHECKSUM_NONE;
    rasta_md4_set_key(&hashing_context, 0, 0, 0, 0);
    h.hashing_context = &hashing_context;

    struct RastaPacket data = createDataMessage(SERVER_ID, 0, 0, 0, 0, 0, app_messages, &hashing_context);
    struct RastaByteArray packet = rastaModuleToBytes(&data, &hashing_context);
    struct RastaByteArray *to_fifo = rmalloc(sizeof(struct RastaByteArray));
    allocateRastaByteArray(to_fifo, packet.length);
    rmemcpy(to_fifo->bytes, packet.bytes, packet.length);
    fifo_push(connection.fifo_retransmission, to_fifo);
    freeRastaByteArray(&packet);

    // Act
    sr_retransmit_data(&connection);

    // Assert

    // Retranmission queue should still contain 1 (unconfirmed) packet
    CU_ASSERT_EQUAL(1, fifo_get_size(connection.fifo_retransmission));

    // Two messages should be sent
    CU_ASSERT_PTR_NOT_NULL_FATAL(test_send_fifo);
    CU_ASSERT_EQUAL(2, fifo_get_size(test_send_fifo));

    struct RastaByteArray *retrdata_message = fifo_pop(test_send_fifo);
    CU_ASSERT_PTR_NOT_NULL(retrdata_message);
    CU_ASSERT_EQUAL(8 + 42, retrdata_message->length);
    CU_ASSERT_EQUAL(RASTA_TYPE_RETRDATA, leShortToHost(retrdata_message->bytes + 8 + 2));
    // Contains 'Hello world'
    CU_ASSERT_EQUAL(message.length, leShortToHost(retrdata_message->bytes + 8 + 28));
    CU_ASSERT_EQUAL(0, memcmp(retrdata_message->bytes + 8 + 28 + 2, message.bytes, message.length));

    struct RastaByteArray *hb_message = fifo_pop(test_send_fifo);
    CU_ASSERT_EQUAL(8 + 28, hb_message->length);
    CU_ASSERT_EQUAL(RASTA_TYPE_HB, leShortToHost(hb_message->bytes + 8 + 2));

    fifo_destroy(&connection.fifo_retransmission);
    fifo_destroy(&test_send_fifo);

    freeRastaByteArray(retrdata_message);
    freeRastaByteArray(hb_message);
    rfree(retrdata_message);
    rfree(hb_message);

    rfree(mux.transport_sockets);
    freeRastaByteArray(&data.data);
    freeRastaByteArray(&hashing_context.key);
    freeRastaByteArray(&fake_channel.hashing_context.key);
    freeRastaByteArray(&mux.sr_hashing_context.key);
}
