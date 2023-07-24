#include <memory.h>
#include <rasta/rasta.h>
#include <rasta/rmemory.h>
#include <scils.h>
#include <scils_telegram_factory.h>
#include <stdio.h>
#include <stdlib.h>

#include "configfile.h"

#define CONFIG_PATH_S "rasta_server_local.cfg"
#define CONFIG_PATH_C "rasta_client_local.cfg"

#define ID_S 0x61
#define ID_C 0x60

#define SCI_NAME_S "S"
#define SCI_NAME_C "C"

#define BUF_SIZE 500

scils_t *scils;

void printHelpAndExit(void) {
    printf("Invalid Arguments!\n use 's' to start in server mode and 'c' to start in client mode.\n");
    exit(1);
}

void send_hp0(void) {
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
    UNUSED(ls);
    printf("Received location status from %s. LS showing main = 0x%02X.\n", sci_get_name_string(sender), signal_aspect.main);
}

int main(int argc, char *argv[]) {
    if (argc != 2) printHelpAndExit();

    rasta_lib_configuration_t rc = {0};
    rasta_ip_data toServer[2];
    unsigned char buf[BUF_SIZE];

    if (strcmp(argv[1], "s") == 0) {
        printf("->   S (ID = 0x%lX)\n", (unsigned long)ID_S);
        rasta_config_info config;
        struct logger_t logger;
        load_configfile(&config, &logger, CONFIG_PATH_S);

        strcpy(toServer[0].ip, "127.0.0.1");
        strcpy(toServer[1].ip, "127.0.0.1");
        toServer[0].port = 9998;
        toServer[1].port = 9999;

        printf("Server at %s:%d and %s:%d\n", toServer[0].ip, toServer[0].port, toServer[1].ip, toServer[1].port);

        rasta_connection_config connection_config = {
            .config = &config,
            .rasta_id = ID_C,
            .transport_sockets = toServer,
            .transport_sockets_count = sizeof(toServer) / sizeof(toServer[0])
        };

        rasta_lib_init_configuration(rc, &config, &logger, &connection_config, 1);
        rasta_bind(rc);
        rasta_listen(rc);

        rasta_connection *connection = rasta_accept(rc);
        if(connection == NULL) {
            printf("Could not accept connection!\n");
            return 1;
        }

        scils = scils_init(&rc->h, SCI_NAME_S);
        scils->notifications.on_show_signal_aspect_received = onShowSignalAspect;
        scils_register_sci_name(scils, SCI_NAME_S, ID_S);
        scils_register_sci_name(scils, SCI_NAME_C, ID_C);

        ssize_t len;
        while((len = rasta_recv(rc, connection, buf, BUF_SIZE)) > 0) {
            rastaApplicationMessage msg = {
                .appMessage = {
                    .bytes = buf, 
                    .length = len
                }, 
                .id = ID_C 
            };
            scils_on_rasta_receive(scils, msg);
        }

        rasta_disconnect(connection);
    } else if (strcmp(argv[1], "c") == 0) {
        printf("->   C (ID = 0x%lX)\n", (unsigned long)ID_C);
        rasta_config_info config;
        struct logger_t logger;
        load_configfile(&config, &logger, CONFIG_PATH_C);

        strcpy(toServer[0].ip, "127.0.0.1");
        strcpy(toServer[1].ip, "127.0.0.1");
        toServer[0].port = 8888;
        toServer[1].port = 8889;

        printf("Client at %s:%d and %s:%d\n", toServer[0].ip, toServer[0].port, toServer[1].ip, toServer[1].port);

        rasta_connection_config connection_config = {
            .config = &config,
            .rasta_id = ID_S,
            .transport_sockets = toServer,
            .transport_sockets_count = sizeof(toServer) / sizeof(toServer[0])
        };

        rasta_lib_init_configuration(rc, &config, &logger, &connection_config, 1);
        rasta_bind(rc);

        rasta_connection *connection = rasta_connect(rc, ID_S);
        if(connection == NULL) {
            printf("Failed to connect any channel!\n");
            return 1;
        }

        scils = scils_init(&rc->h, SCI_NAME_C);
        scils->notifications.on_signal_aspect_status_received = onSignalAspectStatus;
        scils_register_sci_name(scils, SCI_NAME_S, ID_S);
        scils_register_sci_name(scils, SCI_NAME_C, ID_C);

        send_hp0();

        int len = rasta_recv(rc, connection, buf, BUF_SIZE);
        if(len > 0) {
            rastaApplicationMessage msg = {
                .appMessage = {
                    .bytes = buf, 
                    .length = len
                }, 
                .id = ID_S 
            };
            scils_on_rasta_receive(scils, msg);
        }

        rasta_disconnect(connection);
    }

    scils_cleanup(scils);
    rasta_cleanup(rc);
    return 0;
}
