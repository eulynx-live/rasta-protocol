#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rasta/rasta.h>

#include "configfile.h"
#include "wolfssl_certificate_helper.h"

#define CONFIG_PATH_S "rasta_server_local_tls.cfg"
#define CONFIG_PATH_C "rasta_client_local_tls.cfg"

#define BUF_SIZE 500

void prepare_certs(const char *config_path) {
    struct RastaConfig config;
    config_load(&config, config_path);

    // do not overwrite existing certificates, might lead to failure in clients
    if (access(config.values.tls.ca_cert_path, F_OK) || access(config.values.tls.cert_path, F_OK) || access(config.values.tls.key_path, F_OK)) {
        create_certificates(config.values.tls.ca_cert_path, config.values.tls.cert_path, config.values.tls.key_path);

        printf("Generated Certificates");
    }

    dictionary_free(&config.dictionary);
    free(config.values.redundancy.connections.data);
}

void printHelpAndExit(void) {
    printf("Invalid Arguments!\n use 'r' to start in receiver mode and 's' to start in sender mode.\n");
    exit(1);
}

struct connect_event_data {
    rasta *rc;
    struct rasta_connection *connection;
};

int send_input_data(void *carry_data, int fd) {
    (void)fd;
    struct connect_event_data *data = carry_data;
    char buf[BUF_SIZE];
    int c;

    for (;;) {
        size_t read_len = 0;
        while (read_len < BUF_SIZE) {
            c = getchar();

            if (c == EOF) {
                if (read_len > 0) {
                    rasta_send(data->rc, data->connection, buf, read_len);
                }

                rasta_disconnect(data->connection);
                return 1;
            }

            buf[read_len++] = c;

            if (c == '\n') {
                rasta_send(data->rc, data->connection, buf, read_len);
                return 0;
            }
        }
        rasta_send(data->rc, data->connection, buf, read_len);
    }
}

int main(int argc, char *argv[]) {

    if (argc != 2) printHelpAndExit();

    rasta *rc = NULL;

    fd_event input_available_event;
    struct connect_event_data input_available_event_data;

    memset(&input_available_event, 0, sizeof(fd_event));

    input_available_event.callback = send_input_data;
    input_available_event.carry_data = &input_available_event_data;
    input_available_event.fd = STDIN_FILENO;

    char buf[BUF_SIZE];

    if (strcmp(argv[1], "r") == 0) {
        prepare_certs(CONFIG_PATH_S);
        rasta_config_info config;
        struct logger_t logger;
        load_configfile(&config, &logger, CONFIG_PATH_S);
        printf("->   R (ID = 0x%lX)\n", (unsigned long)config.general.rasta_id);

        rc = rasta_lib_init_configuration(&config, LOG_LEVEL_DEBUG, LOGGER_TYPE_CONSOLE);

        rasta_bind(rc);

        rasta_listen(rc);

        struct rasta_connection *c = rasta_accept(rc);
        if (c == NULL) {
            printf("Could not accept connection\n");
            exit(1);
        }

        input_available_event_data.rc = rc;
        input_available_event_data.connection = c;

        enable_fd_event(&input_available_event);
        rasta_add_fd_event(rc, &input_available_event, EV_READABLE);

        ssize_t recv_len;
        while ((recv_len = rasta_recv(rc, c, buf, BUF_SIZE)) > 0) {
            // write to stdout
            if (write(STDOUT_FILENO, buf, recv_len) == -1) {
                break;
            }
        }
    } else if (strcmp(argv[1], "s") == 0) {
        rasta_config_info config;
        struct logger_t logger;
        load_configfile(&config, &logger, CONFIG_PATH_C);
        printf("->   S (ID = 0x%lX)\n", (unsigned long)config.general.rasta_id);

        rc = rasta_lib_init_configuration(&config, LOG_LEVEL_DEBUG, LOGGER_TYPE_CONSOLE);

        rasta_bind(rc);

        struct rasta_connection *c = rasta_connect(rc);

        if (c == NULL) {
            printf("->   Failed to connect any channel.\n");
            return 1;
        };

        input_available_event_data.rc = rc;
        input_available_event_data.connection = c;

        enable_fd_event(&input_available_event);
        rasta_add_fd_event(rc, &input_available_event, EV_READABLE);

        ssize_t recv_len;
        while ((recv_len = rasta_recv(rc, c, buf, BUF_SIZE)) > 0) {
            if (write(STDOUT_FILENO, buf, recv_len) == -1) {
                break;
            }
        }
    }

    rasta_cleanup(rc);
    return 0;
}
