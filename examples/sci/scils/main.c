#include <memory.h>
#include <rasta/rasta.h>
#include <rasta/rmemory.h>
#include <rasta/rasta_lib.h>
#include <scils.h>
#include <scils_telegram_factory.h>
#include <stdio.h>
#include <stdlib.h>

#include "configfile.h"

#define CONFIG_PATH_S "rasta_server_local.cfg"
#define CONFIG_PATH_C "rasta_client_local.cfg"

#define ID_R 0x61
#define ID_S 0x60

#define SCI_NAME_R "R"
#define SCI_NAME_S "S"

#define BUF_SIZE 500

scils_t *scils;

void printHelpAndExit(void) {
    printf("Invalid Arguments!\n use 'r' to start in receiver mode and 's' to start in sender mode.\n");
    exit(1);
}

void onReceive(struct rasta_notification_result *result) {
    rastaApplicationMessage message = sr_get_received_data(result->handle, &result->connection);
    scils_on_rasta_receive(scils, message);
}

void onHandshakeComplete(struct rasta_notification_result *result) {
    if (result->connection.my_id == ID_R) {

        printf("Sending show signal aspect command...\n");
        scils_signal_aspect *signal_aspect = scils_signal_aspect_defaults();
        signal_aspect->main = SCILS_MAIN_HP_0;

        sci_return_code code = scils_send_show_signal_aspect(scils, SCI_NAME_S, *signal_aspect);

        rfree(signal_aspect);
        if (code == SUCCESS) {
            printf("Sent show signal aspect command to server\n");
        } else {
            printf("Something went wrong, error code 0x%02X was returned!\n", code);
        }
    }
}

void onShowSignalAspect(scils_t *ls, char *sender, scils_signal_aspect signal_aspect) {
    printf("Received show signal aspect with MAIN = 0x%02X from %s\n", signal_aspect.main, sci_get_name_string(sender));

    printf("Sending back location status...\n");
    sci_return_code code = scils_send_signal_aspect_status(ls, sender, signal_aspect);
    if (code == SUCCESS) {
        printf("Sent signal aspect status\n");
    } else {
        printf("Something went wrong, error code 0x%02X was returned!\n", code);
    }
}

void onSignalAspectStatus(scils_t *ls, char *sender, scils_signal_aspect signal_aspect) {
    (void)ls;
    printf("Received location status from %s. LS showing main = 0x%02X.\n", sci_get_name_string(sender), signal_aspect.main);
}

struct connect_event_data {
    struct rasta_handle *h;
    fd_event *connect_event;
    fd_event *schwarzenegger;
};

int send_input_data(void *carry_data) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;

    printf("->   Connection request sent to 0x%lX\n", (unsigned long)ID_R);
    struct connect_event_data *data = carry_data;
    sr_connect(data->h, ID_R);
    enable_fd_event(data->schwarzenegger);
    disable_fd_event(data->connect_event);
    return 0;
}

int terminator(void *h) {
    printf("terminating\n");
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
    sr_cleanup(h);
    return 1;
}

int main(int argc, char *argv[]) {

    if (argc != 2) printHelpAndExit();

    rasta_lib_configuration_t rc = {0};

    rasta_ip_data toServer[2];

    fd_event termination_event, connect_on_stdin_event;
    struct connect_event_data connect_on_stdin_event_data = {
        .h = &rc->h,
        .schwarzenegger = &termination_event,
        .connect_event = &connect_on_stdin_event};

    termination_event.callback = terminator;
    termination_event.carry_data = &rc->h;
    termination_event.fd = STDIN_FILENO;

    connect_on_stdin_event.callback = send_input_data;
    connect_on_stdin_event.carry_data = &connect_on_stdin_event_data;
    connect_on_stdin_event.fd = STDIN_FILENO;

    //char buf[BUF_SIZE];

    printf("Server at %s:%d and %s:%d\n", toServer[0].ip, toServer[0].port, toServer[1].ip, toServer[1].port);

    if (strcmp(argv[1], "r") == 0) {
        printf("->   R (ID = 0x%lX)\n", (unsigned long)ID_R);
        rasta_config_info config;
        struct logger_t logger;
        load_configfile(&config, &logger, CONFIG_PATH_S);

        strcpy(toServer[0].ip, "127.0.0.1");
        strcpy(toServer[1].ip, "127.0.0.1");
        toServer[0].port = 9998;
        toServer[1].port = 9999;

        rasta_connection_config connection = {
            .config = &config,
            .rasta_id = ID_R,
            .transport_sockets = toServer,
            .transport_sockets_count = sizeof(toServer) / sizeof(toServer[0])
        };

        // Why was this API so weird? How did all this work back then?
        rasta_lib_init_configuration(rc, &config, &logger, &connection, 1);
        rasta_bind(&rc->h);

        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeComplete;

        scils = scils_init(&rc->h, SCI_NAME_R);
        scils->notifications.on_show_signal_aspect_received = onShowSignalAspect;

        enable_fd_event(&termination_event);
        disable_fd_event(&connect_on_stdin_event);
        add_fd_event(&rc->rasta_lib_event_system, &termination_event, EV_READABLE);
        add_fd_event(&rc->rasta_lib_event_system, &connect_on_stdin_event, EV_READABLE);
        //rasta_recv(rc, 0, true);
    } else if (strcmp(argv[1], "s") == 0) {
        printf("->   S (ID = 0x%lX)\n", (unsigned long)ID_R);
        rasta_config_info config;
        struct logger_t logger;
        load_configfile(&config, &logger, CONFIG_PATH_C);

        strcpy(toServer[0].ip, "127.0.0.1");
        strcpy(toServer[1].ip, "127.0.0.1");
        toServer[0].port = 8888;
        toServer[1].port = 8889;

        rasta_connection_config connection = {
            .config = &config,
            .rasta_id = ID_R,
            .transport_sockets = toServer,
            .transport_sockets_count = sizeof(toServer) / sizeof(toServer[0])
        };

        rasta_lib_init_configuration(rc, &config, &logger, &connection, 1);
        rasta_bind(&rc->h);

        rc->h.notifications.on_receive = onReceive;
        rc->h.notifications.on_handshake_complete = onHandshakeComplete;

        scils = scils_init(&rc->h, SCI_NAME_S);
        scils->notifications.on_signal_aspect_status_received = onSignalAspectStatus;
        scils_register_sci_name(scils, SCI_NAME_R, ID_R);

        printf("->   Press Enter to connect\n");
        disable_fd_event(&termination_event);
        enable_fd_event(&connect_on_stdin_event);
        add_fd_event(&rc->rasta_lib_event_system, &termination_event, EV_READABLE);
        add_fd_event(&rc->rasta_lib_event_system, &connect_on_stdin_event, EV_READABLE);
        //rasta_recv(rc, 0, false);
    }

    getchar();

    scils_cleanup(scils);
    return 0;
}
