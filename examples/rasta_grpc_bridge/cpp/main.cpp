#include <chrono>
#include <condition_variable>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <rasta/rasta_lib.h>
#include <rasta/rasta.h>
#include <rasta/rmemory.h>

#include "configfile.h"

#include <rasta.grpc.pb.h>
#include <rasta.pb.h>

using namespace std::chrono_literals;

static std::mutex s_busy;

// Client
static std::unique_ptr<grpc::ClientContext> s_currentContext;
static std::unique_ptr<grpc::ClientReaderWriter<sci::SciPacket, sci::SciPacket>> s_currentStream;

// Server
static grpc::ServerReaderWriter<sci::SciPacket, sci::SciPacket> *s_currentServerStream;
static grpc::ServerContext *s_currentServerContext;

void *on_con_start(rasta_lib_connection_t connection) {
    (void)connection;
    return malloc(sizeof(rasta_lib_connection_t));
}

void on_con_end(rasta_lib_connection_t connection, void *memory) {
    (void)connection;
    free(memory);
}

struct RastaChannel {
    unsigned long remote_id;
    // The size of subchannels implicitly equals the number of local listen endpoints
    RastaIPData *subchannels;
};

static int accept_event = -1;
static struct rasta_connection accept_connection;
void handle_handshake_complete(struct rasta_notification_result *result) {
    memcpy(&accept_connection, &result->connection, sizeof(struct rasta_connection));
    struct rasta_connection *r = &accept_connection;
    uint64_t ignore = write(accept_event, &r, sizeof(struct rasta_connection *));
    (void)ignore;
}

void handle_connection_state_change(struct rasta_notification_result *result) {
    if (result->connection.current_state == RASTA_CONNECTION_CLOSED) {
        if (accept_event != -1) {
            uint64_t ignore = write(accept_event, (void *)-1, sizeof(struct rasta_connection *));
            (void)ignore;
        }
    }
}

void rasta_listen(rasta_lib_configuration_t rc, const char *config_file_path) {
    struct RastaConfigInfo config;
    struct logger_t logger;
    load_configfile(&config, &logger, config_file_path);
    rasta_lib_init_configuration(rc, config, &logger);
    rc->h.user_handles->on_connection_start = on_con_start;
    rc->h.user_handles->on_disconnect = on_con_end;

    rc->h.notifications.on_handshake_complete = handle_handshake_complete;
    rc->h.notifications.on_connection_state_change = handle_connection_state_change;
    rc->h.notifications.on_receive = [](struct rasta_notification_result *result) {
        static std::mutex s_busy_writing;
        std::lock_guard<std::mutex> guard(s_busy_writing);
        rastaApplicationMessage p;
        p = sr_get_received_data(result->handle, &result->connection);

        const int siz_ar = p.appMessage.length / sizeof(char);

        logger_log(&result->handle->logger, LOG_LEVEL_DEBUG, (char *)"RaSTA retrieve", (char *)"printing message data...");

        for (int i = 0; i < siz_ar; i++)
            fprintf(stdout, "%02X ", p.appMessage.bytes[i]);
        std::cout << std::endl;

        logger_log(&result->handle->logger, LOG_LEVEL_DEBUG, (char *)"RaSTA retrieve", (char *)"printed message data.");

        std::lock_guard<std::mutex> streamGuard(s_busy);
        if (s_currentStream != nullptr) {
            logger_log(&result->handle->logger, LOG_LEVEL_DEBUG, (char *)"RaSTA retrieve", (char *)"forwarding packet to grpc");
            sci::SciPacket outPacket;
            outPacket.set_message(p.appMessage.bytes, p.appMessage.length);
            s_currentStream->Write(outPacket);
        } else if (s_currentServerStream != nullptr) {
            logger_log(&result->handle->logger, LOG_LEVEL_DEBUG, (char *)"RaSTA retrieve", (char *)"forwarding packet to grpc");
            sci::SciPacket outPacket;
            outPacket.set_message(p.appMessage.bytes, p.appMessage.length);
            s_currentServerStream->Write(outPacket);
        } else {
            logger_log(&result->handle->logger, LOG_LEVEL_ERROR, (char *)"RaSTA retrieve", (char *)"discarding packet.");
        }
    };
}

int rasta_accept(rasta_lib_configuration_t rc, struct RastaChannel *channel, struct rasta_connection *connection) {
    struct rasta_connection *existing_connection = NULL;
    for (struct rasta_connection *con = rc->h.first_con; con; con = con->linkedlist_next) {
        if (con->remote_id == channel->remote_id) {
            existing_connection = con;
            break;
        }
    }

    if ((existing_connection == NULL || existing_connection->current_state == RASTA_CONNECTION_CLOSED) && rc->h.config.general.rasta_id < channel->remote_id) {
        // This is a client, initiate handshake
        rc->h.ev_sys = &rc->rasta_lib_event_system;
        sr_connect(&rc->h, channel->remote_id, channel->subchannels);
    }

    // Wait until a handshake has succeeded or the connection dropped to state RASTA_CONNECTION_CLOSED
    accept_event = eventfd(0, 0);
    fd_event fd_event;
    memset(&fd_event, 0, sizeof(fd_event));
    fd_event.callback = [](void *) { return 1; };
    fd_event.carry_data = &rc->h;
    fd_event.fd = accept_event;
    fd_event.enabled = 0;

    enable_fd_event(&fd_event);

    add_fd_event(&rc->rasta_lib_event_system, &fd_event, EV_READABLE);
    if (rc->h.config.general.rasta_id < channel->remote_id) {
        rasta_lib_start(rc, 20000, false);
    } else {
        // Wait for channel establishment indefinitely (server)
        rasta_lib_start(rc, 0, true);
    }
    remove_fd_event(&rc->rasta_lib_event_system, &fd_event);

    existing_connection = NULL;
    for (struct rasta_connection *con = rc->h.first_con; con; con = con->linkedlist_next) {
        if (con->remote_id == channel->remote_id) {
            existing_connection = con;
            break;
        }
    }

    if (existing_connection != NULL &&
        (existing_connection->current_state == RASTA_CONNECTION_START || existing_connection->current_state == RASTA_CONNECTION_CLOSED)) {
        // Connection request timeout
        if (existing_connection->current_state == RASTA_CONNECTION_START) {
            sr_disconnect(&rc->h, existing_connection);
        }
        return false;
    }

    struct rasta_connection *result;
    uint64_t ignore = read(accept_event, &result, sizeof(struct rasta_connection *));
    (void)ignore;
    if (result == &accept_connection) {
        memcpy(connection, &accept_connection, sizeof(struct rasta_connection));
    }
    close(accept_event);
    accept_event = -1;

    return result == &accept_connection;
}

static uint32_t s_remote_id = 0;

static int s_terminator_fd;
static int s_data_fd;

static std::mutex s_fifo_mutex;
static fifo_t *s_message_fifo;

void processRasta(std::string config,
                  std::string rasta_channel1_address, std::string rasta_channel1_port,
                  std::string rasta_channel2_address, std::string rasta_channel2_port,
                  std::string rasta_local_id, std::string rasta_target_id, std::function<std::thread()> run_thread) {
    (void)rasta_local_id;

    // Channels
    struct RastaIPData toServer[2];
    strcpy(toServer[0].ip, rasta_channel1_address.c_str());
    toServer[0].port = std::stoi(rasta_channel1_port);
    strcpy(toServer[1].ip, rasta_channel2_address.c_str());
    toServer[1].port = std::stoi(rasta_channel2_port);

    struct RastaChannel channel;
    channel.remote_id = s_remote_id = std::stoul(rasta_target_id);
    channel.subchannels = toServer;

    rasta_lib_configuration_t rc;
    rasta_listen(rc, config.c_str());

    while (true) {
        struct rasta_connection new_connection;
        if (rasta_accept(rc, &channel, &new_connection)) {
            s_message_fifo = fifo_init(128);

            // Data event
            s_data_fd = eventfd(0, 0);
            fd_event data_event;
            memset(&data_event, 0, sizeof(fd_event));
            data_event.callback = [](void *carry) {
                rasta_handle *h = reinterpret_cast<rasta_handle *>(carry);
                RastaByteArray *msg = nullptr;

                {
                    std::lock_guard<std::mutex> guard(s_fifo_mutex);
                    msg = reinterpret_cast<RastaByteArray *>(fifo_pop(s_message_fifo));
                }

                if (msg != nullptr) {
                    struct RastaMessageData messageData;
                    allocateRastaMessageData(&messageData, 1);
                    messageData.data_array[0] = *msg;
                    rfree(msg);

                    sr_send(h, s_remote_id, messageData);

                    freeRastaMessageData(&messageData); // Also frees the byte array
                }

                return 0;
            };
            data_event.carry_data = &rc->h;
            data_event.fd = s_data_fd;
            enable_fd_event(&data_event);
            add_fd_event(&rc->rasta_lib_event_system, &data_event, EV_READABLE);

            // Terminator event
            s_terminator_fd = eventfd(0, 0);
            fd_event terminator_event;
            memset(&terminator_event, 0, sizeof(fd_event));
            terminator_event.callback = [](void *carry) {
                rasta_handle *h = reinterpret_cast<rasta_handle *>(carry);
                struct rasta_connection *existing_connection = NULL;
                for (struct rasta_connection *con = h->first_con; con; con = con->linkedlist_next) {
                    if (con->remote_id == s_remote_id) {
                        existing_connection = con;
                        break;
                    }
                }
                if (existing_connection != NULL) {
                    sr_disconnect(h, existing_connection);
                }
                return 1;
            };
            terminator_event.carry_data = &rc->h;
            terminator_event.fd = s_terminator_fd;
            enable_fd_event(&terminator_event);
            add_fd_event(&rc->rasta_lib_event_system, &terminator_event, EV_READABLE);

            auto forwarderThread = run_thread();

            rasta_lib_start(rc, 0, false);

            {
                std::lock_guard<std::mutex> guard(s_busy);
                if (s_currentContext) {
                    s_currentContext->TryCancel();
                } else if (s_currentServerContext) {
                    s_currentServerContext->TryCancel();
                }
            }

            forwarderThread.join();

            remove_fd_event(&rc->rasta_lib_event_system, &data_event);
            remove_fd_event(&rc->rasta_lib_event_system, &terminator_event);

            close(s_data_fd);
            close(s_terminator_fd);

            fifo_destroy(s_message_fifo);
        }

        // Give the remote the chance to notice the possibly broken connection
        sleep(1);
    }

    sr_cleanup(&rc->h);
}

class RastaService final : public sci::Rasta::Service {
  public:
    RastaService(std::string config,
                 std::string rasta_channel1_address, std::string rasta_channel1_port,
                 std::string rasta_channel2_address, std::string rasta_channel2_port,
                 std::string rasta_local_id, std::string rasta_target_id)
        : _config(config), _rasta_channel1_address(rasta_channel1_address), _rasta_channel1_port(rasta_channel1_port), _rasta_channel2_address(rasta_channel2_address), _rasta_channel2_port(rasta_channel2_port), _rasta_local_id(rasta_local_id), _rasta_target_id(rasta_target_id) {}

    grpc::Status Stream(grpc::ServerContext *context, grpc::ServerReaderWriter<sci::SciPacket, sci::SciPacket> *stream) override {
        {
            std::lock_guard<std::mutex> guard(s_busy);
            s_currentServerContext = context;
            s_currentServerStream = stream;
        }

        auto forwardGrpc = [&]() {
            return std::thread([&]() {
                sci::SciPacket message;
                while (s_currentServerStream->Read(&message)) {
                    printf("Forwarding gRPC message...\n");
                    struct RastaByteArray *msg = reinterpret_cast<RastaByteArray *>(rmalloc(sizeof(struct RastaByteArray)));
                    allocateRastaByteArray(msg, message.message().size());
                    rmemcpy(msg->bytes, message.message().c_str(), message.message().size());

                    {
                        std::lock_guard<std::mutex> guard(s_fifo_mutex);
                        fifo_push(s_message_fifo, msg);
                    }

                    uint64_t notify_data = 1;
                    uint64_t ignore = write(s_data_fd, &notify_data, sizeof(uint64_t));
                    (void)ignore;
                }

                uint64_t terminate = 1;
                uint64_t ignore = write(s_terminator_fd, &terminate, sizeof(uint64_t));
                (void)ignore;
            });
        };

        processRasta(_config, _rasta_channel1_address, _rasta_channel1_port, _rasta_channel2_address, _rasta_channel2_port, _rasta_local_id, _rasta_target_id, forwardGrpc);

        {
            std::lock_guard<std::mutex> guard(s_busy);
            s_currentServerContext = nullptr;
            s_currentServerStream = nullptr;
        }

        return grpc::Status::OK;
    }

  protected:
    std::string _config;
    std::string _rasta_channel1_address;
    std::string _rasta_channel1_port;
    std::string _rasta_channel2_address;
    std::string _rasta_channel2_port;
    std::string _rasta_local_id;
    std::string _rasta_target_id;
};

int main(int argc, char *argv[]) {
    if (argc < 9) {
        std::cout << "Usage: " << argv[0] << " <config_file> <listen_address> <target_host_ch0> <target_port_ch0> <target_host_ch1> <target_port_ch1> <local_rasta_id> <local_remote_id> <?grpc_target_address>" << std::endl;
        return 1;
    }

    std::string config(argv[1]);

    struct stat buffer;
    if (stat(config.c_str(), &buffer) < 0) {
        std::cerr << "Could not open \"" << config << "\"." << std::endl;
        return 1;
    }

    std::string server_address(argv[2]);
    std::string rasta_channel1_address(argv[3]);
    std::string rasta_channel1_port(argv[4]);
    std::string rasta_channel2_address(argv[5]);
    std::string rasta_channel2_port(argv[6]);
    std::string rasta_local_id(argv[7]);
    std::string rasta_target_id(argv[8]);
    std::string grpc_server_address;
    if (argc >= 10) {
        grpc_server_address = std::string(argv[9]);
    }

    if (grpc_server_address.length() == 0) {
        // Start a gRPC server and wait for incoming connection before doing anything RaSTA
        RastaService svc(config,
                         rasta_channel1_address, rasta_channel1_port,
                         rasta_channel2_address, rasta_channel2_port,
                         rasta_local_id, rasta_target_id);

        grpc::EnableDefaultHealthCheckService(true);
        grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        grpc::ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *synchronous* service.
        builder.RegisterService(&svc);
        // Finally assemble the server.
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::cout << "Server listening on " << server_address << std::endl;

        // Wait for the server to shutdown. Note that some other thread must be
        // responsible for shutting down the server for this call to ever return.
        server->Wait();
    } else {
        auto connectGrpc = [&]() {
            return std::thread([&]() {
                printf("Creating gRPC connection to %s...\n", grpc_server_address.c_str());
                auto channel = grpc::CreateChannel(grpc_server_address, grpc::InsecureChannelCredentials());
                auto stub = sci::Rasta::NewStub(channel);

                {
                    // Establish gRPC connection
                    std::lock_guard<std::mutex> guard(s_busy);
                    s_currentContext = std::make_unique<grpc::ClientContext>();
                    s_currentContext->AddMetadata("rasta-id", std::to_string(s_remote_id));
                    s_currentStream = stub->Stream(s_currentContext.get());
                }

                sci::SciPacket message;
                while (s_currentStream->Read(&message)) {
                    printf("Forwarding gRPC message...\n");
                    struct RastaByteArray *msg = reinterpret_cast<RastaByteArray *>(rmalloc(sizeof(struct RastaByteArray)));
                    allocateRastaByteArray(msg, message.message().size());
                    rmemcpy(msg->bytes, message.message().c_str(), message.message().size());

                    {
                        std::lock_guard<std::mutex> guard(s_fifo_mutex);
                        fifo_push(s_message_fifo, msg);
                    }

                    uint64_t notify_data = 1;
                    uint64_t ignore = write(s_data_fd, &notify_data, sizeof(uint64_t));
                    (void)ignore;
                }

                {
                    std::lock_guard<std::mutex> guard(s_busy);
                    s_currentStream = nullptr;
                    s_currentContext = nullptr;
                }

                uint64_t terminate = 1;
                uint64_t ignore = write(s_terminator_fd, &terminate, sizeof(uint64_t));
                (void)ignore;
            });
        };

        // Establish a RaSTA connection and connect to gRPC server afterwards
        processRasta(config,
                     rasta_channel1_address, rasta_channel1_port,
                     rasta_channel2_address, rasta_channel2_port,
                     rasta_local_id, rasta_target_id, connectGrpc);
    }
    return 0;
}
