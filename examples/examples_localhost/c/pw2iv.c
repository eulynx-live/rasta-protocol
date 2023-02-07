//
// Created by erica on 02.08.22.
//

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define htole32(X) (X)

static void usage(const char *name) {
    fprintf(stderr, "Usage: %s <password>\n", name);
    exit(1);
}

int main(int argc, char *argv[]) {
    size_t pw_len, pw_written;
    const char *pw;
    char *last_word;

    const uint32_t MD4_STD_A = 0x67452301,
                   MD4_STD_B = 0xefcdab89,
                   MD4_STD_C = 0x98badcfe,
                   MD4_STD_D = 0x10325476;

    const size_t MD4_IV_WORDS = 4;
    uint32_t output[MD4_IV_WORDS];

    if (argc == 1) {
        usage(argv[0]);
    }

    pw = argv[1];

    pw_len = strlen(pw);

    if (pw_len > sizeof(output)) {
        fprintf(stderr, "Error: maximum password length is %lu bytes\n", sizeof(output));
        return 1;
    }

    output[0] = htole32(MD4_STD_A);
    output[1] = htole32(MD4_STD_B);
    output[2] = htole32(MD4_STD_C);
    output[3] = htole32(MD4_STD_D);

    for (pw_written = 0; pw_written < pw_len - (pw_len % 4); pw_written += 4) {
        output[pw_written / 4] = htole32(*(uint32_t *)&pw[pw_written]);
    }

    last_word = (char *)&output[pw_len / 4];
    for (size_t i = 0; i < pw_len - pw_written; i++) {
        last_word[pw_len - pw_written - i - 1] = pw[pw_len - i - 1];
    }
    printf("; std: 0x%" PRIx32 "\n", MD4_STD_A);
    printf("RASTA_MD4_A = #%" PRIx32 "\n", output[0]);
    printf("; std: 0x%" PRIx32 "\n", MD4_STD_B);
    printf("RASTA_MD4_A = #%" PRIx32 "\n", output[1]);
    printf("; std: 0x%" PRIx32 "\n", MD4_STD_C);
    printf("RASTA_MD4_A = #%" PRIx32 "\n", output[2]);
    printf("; std: 0x%" PRIx32 "\n", MD4_STD_D);
    printf("RASTA_MD4_A = #%" PRIx32 "\n", output[3]);

    return 0;
}
