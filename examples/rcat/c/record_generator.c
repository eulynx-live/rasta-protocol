//
// Created by erica on 05/07/2022.
//

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../../src/c/experimental/key_exchange.h"
#include <string.h>

#define MAX_PASSWORD_LENGTH 128

void usage(const char *name) {
    fprintf(stderr, "Usage: %s -m <server RaSTA id in hex> -r <remote RaSTA id in hex> [-p <passphrase> or read from stdin]\n", name);
    exit(1);
}

int main(int argc, char *argv[]) {
    char passphrase_buffer[MAX_PASSWORD_LENGTH];
    const char *input_passphrase = NULL;
    char *endptr = NULL;
    struct key_exchange_state kex_state;
    int64_t my_id = -1, remote_id = -1;

    struct logger_t logger;
    logger_init(&logger, LOG_LEVEL_ERROR, LOGGER_TYPE_CONSOLE);

    int ret, opt;

    while ((opt = getopt(argc, argv, "m:r:p:")) != -1) {
        switch (opt) {
        case 'm':
            my_id = strtol(optarg, &endptr, 16);
            if (*endptr) {
                fprintf(stderr, "Invalid server ID: %s", optarg);
                usage(argv[0]);
            }
            break;
        case 'r':
            remote_id = strtol(optarg, &endptr, 16);
            if (*endptr) {
                fprintf(stderr, "Invalid remote ID: %s", optarg);
                usage(argv[0]);
            }
            break;
        case 'p':
            input_passphrase = optarg;
            break;
        default:
            fprintf(stderr, "Unknown option %c\n", (char)opt);
            usage(argv[0]);
        }
    }

    if (!input_passphrase) {
        if (!fgets(passphrase_buffer, MAX_PASSWORD_LENGTH, stdin)) {
            fprintf(stderr, "Could not read passphrase from stdin!\n");
            return 1;
        }

        // remove trailing newline character if present, so that the PSK is interoperable with the config file
        passphrase_buffer[strcspn(passphrase_buffer, "\n")] = '\0';
        input_passphrase = passphrase_buffer;
    }

    if (my_id == -1 || remote_id == -1) {
        fprintf(stderr, "Invalid server or remote ID!\n");
        usage(argv[0]);
    }

    ret = key_exchange_prepare_from_psk(&kex_state, input_passphrase, my_id, remote_id, &logger);

    if (ret) {
        fprintf(stderr, "Error creating record!\n");
        return 1;
    }

    printf("%s", CONFIGURATION_FILE_USER_RECORD_HEADER);

    for (size_t i = 0; i < sizeof(kex_state.user_record); i++) {
        printf("%02" PRIx8, kex_state.user_record[i]);
    }

    return 0;
}
