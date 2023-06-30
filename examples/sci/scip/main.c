#include <memory.h>
#include <rasta/rasta.h>
#include <rasta/rmemory.h>
#include <scip.h>
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

scip_t *scip;

void printHelpAndExit(void) {
    printf("Invalid Arguments!\n use 's' to start in server mode and 'c' to start in client mode.\n");
    exit(1);
}

void send_point_to_right(void) {
    printf("Sending change location command...\n");
    sci_return_code code = scip_send_change_location(scip, SCI_NAME_S, POINT_LOCATION_CHANGE_TO_RIGHT);
    if (code == SUCCESS) {
        printf("Sent change location command to server\n");
    } else {
        printf("Something went wrong, error code 0x%02X was returned!\n", code);
    }
}

void onChangeLocation(scip_t *p, char *sender, scip_point_target_location location) {
    printf("Received location change to 0x%02X from %s\n", location, sci_get_name_string(sender));

    printf("Sending back location status...\n");
    sci_return_code code = scip_send_location_status(p, sender, POINT_LOCATION_RIGHT);
    if (code == SUCCESS) {
        printf("Sent location status\n");
    } else {
        printf("Something went wrong, error code 0x%02X was returned!\n", code);
    }
}

void onLocationStatus(scip_t *p, char *sender, scip_point_location location) {
    UNUSED(p);
    printf("Received location status from %s. Point is at position 0x%02X.\n", sci_get_name_string(sender), location);
}

int main(int argc, char *argv[]) {
    if (argc != 2) printHelpAndExit();

    rasta_lib_configuration_t rc = {0};
    struct rasta_ip_data toServer[2];
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
        rasta_bind(&rc->h);
        sr_listen(&rc->h);

        rasta_connection *connection = rasta_accept(rc);
        if(connection == NULL) {
            printf("Could not accept connection!\n");
            return 1;
        }

        scip = scip_init(&rc->h, SCI_NAME_S);
        scip->notifications.on_change_location_received = onChangeLocation;
        scip_register_sci_name(scip, SCI_NAME_S, ID_S);
        scip_register_sci_name(scip, SCI_NAME_C, ID_C);

        ssize_t len;
        while((len = rasta_recv(rc, connection, buf, BUF_SIZE)) > 0) {
            rastaApplicationMessage msg = {
                .appMessage = {
                    .bytes = buf, 
                    .length = len
                }, 
                .id = ID_C 
            };
            scip_on_rasta_receive(scip, msg);
        }

        // sr_disconnect(connection);
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
        rasta_bind(&rc->h);

        rasta_connection *connection = sr_connect(&rc->h, ID_S);
        if(connection == NULL) {
            printf("Failed to connect any channel!\n");
            return 1;
        }

        scip = scip_init(&rc->h, SCI_NAME_C);
        scip->notifications.on_location_status_received = onLocationStatus;
        scip_register_sci_name(scip, SCI_NAME_S, ID_S);
        scip_register_sci_name(scip, SCI_NAME_C, ID_C);

        send_point_to_right();

        int len = rasta_recv(rc, connection, buf, BUF_SIZE);
        if(len > 0) {
            rastaApplicationMessage msg = {
                .appMessage = {
                    .bytes = buf, 
                    .length = len
                }, 
                .id = ID_S 
            };
            scip_on_rasta_receive(scip, msg);
        }

        //sr_disconnect(connection);
    }

    scip_cleanup(scip);
    sr_cleanup(&rc->h);
    return 0;
}
