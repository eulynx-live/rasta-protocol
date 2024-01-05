#include "configfile.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <rasta/config.h>

#include "../../../src/c/experimental/key_exchange.h"
#include "../../../src/c/util/rastacrc.h"

struct LineParser {
    char buf[CONFIG_BUFFER_LENGTH];
    unsigned int pos;
    char current;
    int line;
    struct RastaConfig *cfg;
};

/*
 * Private parser functions
 */
/**
 * initializes the parser and copies the line in the parser
 * @param p
 * @param line
 * @param n_line linenumber
 */
void parser_init(struct LineParser *p, const char line[CONFIG_BUFFER_LENGTH], int n_line, struct RastaConfig *cfg) {
    p->pos = 0;
    strcpy(p->buf, line);
    p->current = p->buf[0];
    p->cfg = cfg;
    p->line = n_line;
}

/**
 * increases the position of the parser and set current to the current char
 * @param p
 * @return 1 if successful, 0 if end of line
 */
int parser_next(struct LineParser *p) {
    p->pos = p->pos + 1;
    if (p->pos >= CONFIG_BUFFER_LENGTH || p->pos >= strlen(p->buf) || p->buf[p->pos] == '\n') {
        p->pos = CONFIG_BUFFER_LENGTH;
        return 0;
    } else {
        p->current = p->buf[p->pos];
        return 1;
    }
}

/**
 * increases the parser position until
 * @param p
 */
void parser_skipBlank(struct LineParser *p) {
    while (p->current == ' ' || p->current == '\t') {
        if (!parser_next(p)) {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Reached unexpected end of line", p->line);
            return;
        }
    }
}

/**
 * parses the identifier with maxlength MAX_DICTIONARY_STRING_LENGTH_BYTES characters
 * @param p
 * @param identifier pointer to the output
 */
void parser_parseIdentifier(struct LineParser *p, char *identifier) {
    parser_skipBlank(p);
    int i = 0;
    while (isdigit(p->current) || isalpha(p->current) || (p->current == '_')) {
        if (i >= MAX_DICTIONARY_STRING_LENGTH_BYTES - 1) {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Identifiers is too long", p->line);
            return;
        }
        if (isalpha(p->current)) {
            identifier[i] = (char)toupper(p->current);
        } else
            identifier[i] = p->current;
        i++;
        if (!parser_next(p)) {
            break;
        }
    }
    identifier[i] = '\0';
}

/**
 * parses a number literal
 * @param p
 * @param number pointer to number
 * @return 1 if successful 0 else
 */
int parser_parseNumber(struct LineParser *p, int *number) {
    int neg = 1;
    if (p->current == '-') {
        neg = -1;
        parser_next(p);
    }
    if (!isdigit(p->current)) {
        logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Expected a digit after '-'", p->line);
        return 0;
    }

    char num_buf[100];
    int i = 0;
    while (isdigit(p->current)) {
        if (i >= 100) {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Number is too long", p->line);
            return 0;
        }
        num_buf[i] = p->current;
        i++;
        if (!parser_next(p)) {
            break;
        }
    }
    num_buf[i] = '\0';

    *number = neg * atoi(num_buf);
    return 1;
}

/**
 * parses a string
 * @param p
 * @param string pointer to string
 * @return 1 if successful 0 else
 */
int parser_parseString(struct LineParser *p, char *string) {
    if (p->current == '"') parser_next(p);

    int i = 0;

    while (p->current != '"') {
        if (i >= MAX_DICTIONARY_STRING_LENGTH_BYTES - 1) {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: String is too long", p->line);
            return 0;
        }
        string[i] = p->current;

        if (!parser_next(p)) {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Missing closing '\"'", p->line);
            return 0;
        }
        i++;
    }
    string[i] = '\0';
    return 1;
}

/**
 * parses a hex
 * @param p
 * @param hex
 * @return
 */
int parser_parseHex(struct LineParser *p, int *hex) {
    if (p->current == '#') parser_next(p);

    char num_buf[100];
    char c;
    int i = 0;

    if (isalpha(p->current))
        c = (char)tolower(p->current);
    else
        c = p->current;

    while (isdigit(c) || c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f') {
        if (i >= 100) {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Hex is too long", p->line);
            return 0;
        }
        num_buf[i] = c;
        i++;

        if (!parser_next(p)) {
            break;
        }

        if (isalpha(p->current))
            c = (char)tolower(p->current);
        else
            c = p->current;
    }
    num_buf[i] = '\0';

    *hex = (int)strtol(num_buf, NULL, 16);
    return 1;
}
/**
 * parses an array
 * @param p
 * @param array array must be allocated with size 1
 * @return
 */
int parser_parseArray(struct LineParser *p, struct DictionaryArray *array) {
    if (p->current == '{') parser_next(p);

    unsigned int i = 0;
    while (p->current != '}') {
        parser_skipBlank(p);
        // skip number arrays
        if (p->current == '0' || p->current == '1' || p->current == '2' || p->current == '3' || p->current == '4' || p->current == '5' || p->current == '6' || p->current == '7' || p->current == '8' || p->current == '9') {
            return 1;
        }

        if (p->current != '"') {
            logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Expected '\"' but found %c", p->line, p->current);
            return 0;
        }

        char string[MAX_DICTIONARY_STRING_LENGTH_BYTES];
        if (!parser_parseString(p, string)) {
            return 0;
        }

        // string is okay, lets check the arrays size
        if (array->count <= i) reallocate_DictionaryArray(array, i + 1);
        strcpy(array->data[i].c, string);
        i++;

        if (p->current == '"') parser_next(p);
        parser_skipBlank(p);
        if (p->current == ';' || p->current == ',') {
            parser_next(p);
            continue;
        } else {
            if (p->current == '}')
                break;
            else {
                logger_log(&p->cfg->logger, LOG_LEVEL_ERROR, p->cfg->filename, "Error in line %d: Expected ';' or '}'", p->line);
                return 0;
            }
        }
    }

    return 1;
}

/**
 * parses the value and adds the key value pair to the dictionary
 * @param p
 * @param key
 * @return
 */
void parser_parseValue(struct LineParser *p, const char key[MAX_DICTIONARY_STRING_LENGTH_BYTES]) {
    // skip empty start
    parser_skipBlank(p);

    if (p->current == '-' || isdigit(p->current)) {
        // parse number
        int number;
        if (parser_parseNumber(p, &number)) {
            dictionary_addNumber(&p->cfg->dictionary, key, number);
        }
    } else if (p->current == '"') {
        // parse string
        struct DictionaryString string;
        if (parser_parseString(p, string.c)) {
            dictionary_addString(&p->cfg->dictionary, key, string);
        }
    } else if (p->current == '{') {
        // parse array
        struct DictionaryArray array;
        array = allocate_DictionaryArray(1);
        if (parser_parseArray(p, &array)) {
            dictionary_addArray(&p->cfg->dictionary, key, array);
        } else {
            free_DictionaryArray(&array);
        }
    } else if (p->current == '#') {
        int hex;
        if (parser_parseHex(p, &hex)) {
            dictionary_addNumber(&p->cfg->dictionary, key, hex);
        }
    } else {
        // parse identifier
        struct DictionaryString identifier;
        parser_parseIdentifier(p, identifier.c);
        dictionary_addString(&p->cfg->dictionary, key, identifier);
    }
}

/**
 * accepts a string like 192.168.2.1:80 and returns the record
 * @param data
 * @return the record. Port is set to 0 if wrong format
 */
rasta_ip_data extractIPData(char data[256]) {
    int points = 0;
    int numbers = 0;
    int pos = 0;
    char port[10];
    rasta_ip_data result;

    // check ip format
    for (unsigned int i = 0; i < strlen(data); i++) {
        if (isdigit(data[i])) {
            numbers++;
            if (numbers > 3) {
                result.port = 0;
                return result;
            }
            result.ip[i] = data[i];
        } else if (data[i] == '.') {
            numbers = 0;
            points++;
            if (points > 3) {
                result.port = 0;
                return result;
            }
            result.ip[i] = data[i];
        } else if (data[i] == ':') {
            if (points == 3 && numbers > 0) {
                pos = i;
                result.ip[i] = '\0';
                break;
            }
        } else {
            result.port = 0;
            return result;
        }
    }

    // get port
    int j = 0;
    for (unsigned int i = pos + 1; i < strlen(data); i++) {
        if (isdigit(data[i])) {
            port[j] = data[i];
        } else {
            result.port = 0;
            return result;
        }
        j++;
    }
    port[j] = '\0';
    result.port = atoi(port);
    return result;
}

#define stringify(s) #s

/**
 * sets the standard values in config
 * @param cfg
 */
void config_setstd(struct RastaConfig *cfg) {
    struct DictionaryEntry entr;

    /*
     * sending part
     */

    // tmax
    entr = config_get(cfg, "RASTA_T_MAX");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.sending.t_max = 1800;
    } else {
        // check valid format
        cfg->values.sending.t_max = (unsigned int)entr.value.number;
    }

    // t_h
    entr = config_get(cfg, "RASTA_T_H");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.sending.t_h = 300;
    } else {
        // check valid format
        cfg->values.sending.t_h = (unsigned int)entr.value.number;
    }

    // checksum type
    entr = config_get(cfg, "RASTA_MD4_TYPE");

    // the RASTA_MD4_TYPE key is only used for compatibility reasons, otherwise its called RASTA_SR_CHECKSUM_LEN
    if (entr.type == DICTIONARY_ERROR) {
        entr = config_get(cfg, "RASTA_SR_CHECKSUM_LEN");
    }

    if (entr.type != DICTIONARY_STRING) {
        // set std
        cfg->values.sending.md4_type = RASTA_CHECKSUM_8B;
    } else {
        // check right parameters
        if (strcmp(entr.value.string.c, "NONE") == 0) {
            cfg->values.sending.md4_type = RASTA_CHECKSUM_NONE;
        } else if (strcmp(entr.value.string.c, "HALF") == 0) {
            cfg->values.sending.md4_type = RASTA_CHECKSUM_8B;
        } else if (strcmp(entr.value.string.c, "FULL") == 0) {
            cfg->values.sending.md4_type = RASTA_CHECKSUM_16B;
        } else {
            // set std
            logger_log(&cfg->logger, LOG_LEVEL_ERROR, cfg->filename, "RASTA_MD4_TYPE or RASTA_SR_CHECKSUM_LEN  may only be NONE, HALF or FULL");
            cfg->values.sending.md4_type = RASTA_CHECKSUM_8B;
        }
    }

    // hash function key
    entr = config_get(cfg, "RASTA_SR_CHECKSUM_KEY");
    // only accept numbers
    if (entr.type == DICTIONARY_NUMBER) {
        cfg->values.sending.sr_hash_key = (unsigned int)entr.value.number;
    }

    // hash algorithm
    entr = config_get(cfg, "RASTA_SR_CHECKSUM_ALGO");
    if (entr.type != DICTIONARY_STRING) {
        // set MD4 as default value
        cfg->values.sending.sr_hash_algorithm = RASTA_ALGO_MD4;
    } else {
        if (strcmp(entr.value.string.c, "MD4") == 0) {
            cfg->values.sending.sr_hash_algorithm = RASTA_ALGO_MD4;
        } else if (strcmp(entr.value.string.c, "BLAKE2B") == 0) {
            cfg->values.sending.sr_hash_algorithm = RASTA_ALGO_BLAKE2B;
        } else if (strcmp(entr.value.string.c, "SIPHASH-2-4") == 0) {
            cfg->values.sending.sr_hash_algorithm = RASTA_ALGO_SIPHASH_2_4;
        }
    }

    // md4_a
    entr = config_get(cfg, "RASTA_MD4_A");
    if (entr.type != DICTIONARY_NUMBER) {
        // set std
        cfg->values.sending.md4_a = 0x67452301;
    } else {
        // check valid format
        cfg->values.sending.md4_a = (unsigned int)entr.value.number;
    }

    // md4_b
    entr = config_get(cfg, "RASTA_MD4_B");
    if (entr.type != DICTIONARY_NUMBER) {
        // set std
        cfg->values.sending.md4_b = 0xefcdab89;
    } else {
        // check valid format
        cfg->values.sending.md4_b = (unsigned int)entr.value.number;
    }

    // md4_c
    entr = config_get(cfg, "RASTA_MD4_C");
    if (entr.type != DICTIONARY_NUMBER) {
        // set std
        cfg->values.sending.md4_c = 0x98badcfe;
    } else {
        // check valid format
        cfg->values.sending.md4_c = (unsigned int)entr.value.number;
    }

    // md4_d
    entr = config_get(cfg, "RASTA_MD4_D");
    if (entr.type != DICTIONARY_NUMBER) {
        // set std
        cfg->values.sending.md4_d = 0x10325476;
    } else {
        // check valid format
        cfg->values.sending.md4_d = (unsigned int)entr.value.number;
    }

    // sendmax
    entr = config_get(cfg, "RASTA_SEND_MAX");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.sending.send_max = 20;
    } else {
        // check valid format
        cfg->values.sending.send_max = (unsigned short)entr.value.number;
    }

    // mwa
    entr = config_get(cfg, "RASTA_MWA");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.sending.mwa = 10;
    } else {
        // check valid format
        cfg->values.sending.mwa = (unsigned short)entr.value.number;
    }

    // maxpacket
    entr = config_get(cfg, "RASTA_MAX_PACKET");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.sending.max_packet = 3;
    } else {
        // check valid format
        cfg->values.sending.max_packet = (unsigned int)entr.value.number;
    }

    // diagwindow
    entr = config_get(cfg, "RASTA_DIAG_WINDOW");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.sending.diag_window = 5000;
    } else {
        // check valid format
        cfg->values.sending.diag_window = (unsigned int)entr.value.number;
    }

    /*
     * Receive part
     */

    entr = config_get(cfg, "RASTA_RECVQUEUE_SIZE");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.receive.max_recvqueue_size = 20;
    } else {
        // check valid format
        cfg->values.receive.max_recvqueue_size = (unsigned int)entr.value.number;
    }

    /*
     * Receive recv message size
     */

    entr = config_get(cfg, "RASTA_RECV_MSG_SIZE");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.receive.max_recv_msg_size = 500;
    } else {
        // check valid format
        cfg->values.receive.max_recv_msg_size = (unsigned int)entr.value.number;
    }

    /*
     * Retransmission part
     */

    entr = config_get(cfg, "RASTA_RETRANSMISSION_QUEUE_SIZE");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.retransmission.max_retransmission_queue_size = 100;
    } else {
        // check valid format
        cfg->values.retransmission.max_retransmission_queue_size = (unsigned int)entr.value.number;
    }

    /*
     * Redundancy part
     */

    // redundancy channels
    entr = config_get(cfg, "RASTA_REDUNDANCY_CONNECTIONS");
    if (entr.type != DICTIONARY_ARRAY || entr.value.array.count == 0) {
        // set std
        cfg->values.redundancy.connections.count = 0;
    } else {
        cfg->values.redundancy.connections.data = malloc(sizeof(rasta_ip_data) * entr.value.array.count);
        cfg->values.redundancy.connections.count = entr.value.array.count;
        // check valid format
        for (unsigned int i = 0; i < entr.value.array.count; i++) {
            rasta_ip_data ip = extractIPData(entr.value.array.data[i].c);
            if (ip.port == 0) {
                logger_log(&cfg->logger, LOG_LEVEL_ERROR, cfg->filename, "RASTA_REDUNDANCY_CONNECTIONS may only contain strings in format ip:port or *:port");
                free(entr.value.array.data);
                entr.value.array.count = 0;
                break;
            }
            cfg->values.redundancy.connections.data[i] = ip;
        }
    }

    // crc type
    entr = config_get(cfg, "RASTA_CRC_TYPE");
    if (entr.type != DICTIONARY_STRING) {
        // set std
        cfg->values.redundancy.crc_type = crc_init_opt_a();
    } else {
        // check right parameters
        if (strcmp(entr.value.string.c, "TYPE_A") == 0) {
            cfg->values.redundancy.crc_type = crc_init_opt_a();
        } else if (strcmp(entr.value.string.c, "TYPE_B") == 0) {
            cfg->values.redundancy.crc_type = crc_init_opt_b();
        } else if (strcmp(entr.value.string.c, "TYPE_C") == 0) {
            cfg->values.redundancy.crc_type = crc_init_opt_c();
        } else if (strcmp(entr.value.string.c, "TYPE_D") == 0) {
            cfg->values.redundancy.crc_type = crc_init_opt_d();
        } else if (strcmp(entr.value.string.c, "TYPE_E") == 0) {
            cfg->values.redundancy.crc_type = crc_init_opt_e();
        } else {
            // set std
            logger_log(&cfg->logger, LOG_LEVEL_ERROR, cfg->filename, "RASTA_CRC_TYPE may only be TYPE_A, TYPE_B, TYPE_C, TYPE_D or TYPE_E");
            cfg->values.redundancy.crc_type = crc_init_opt_a();
        }
    }

    // tseq
    entr = config_get(cfg, "RASTA_T_SEQ");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.redundancy.t_seq = 100;
    } else {
        // check valid format
        cfg->values.redundancy.t_seq = (unsigned short)entr.value.number;
    }

    // ndiagnose
    entr = config_get(cfg, "RASTA_N_DIAGNOSE");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.redundancy.n_diagnose = 200;
    } else {
        // check valid format
        cfg->values.redundancy.n_diagnose = (unsigned short)entr.value.number;
    }

    // ndeferqueue
    entr = config_get(cfg, "RASTA_N_DEFERQUEUE_SIZE");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.redundancy.n_deferqueue_size = 4;
    } else {
        // check valid format
        cfg->values.redundancy.n_deferqueue_size = (unsigned short)entr.value.number;
    }

    /*
     * General
     */

    // network
    entr = config_get(cfg, "RASTA_NETWORK");
    if (entr.type != DICTIONARY_NUMBER || entr.value.number < 0) {
        // set std
        cfg->values.general.rasta_network = 0;
    } else {
        // check valid format
        cfg->values.general.rasta_network = (unsigned long)entr.value.number;
    }

    // receiver
    entr = config_get(cfg, "RASTA_ID");
    if (entr.type != DICTIONARY_NUMBER) {
        // set std
        cfg->values.general.rasta_id = 0;
    } else {
        // check valid format
        cfg->values.general.rasta_id = (unsigned long)entr.value.unumber;
    }

    // TLS settings

    entr = config_get(cfg, "RASTA_CA_PATH");
    cfg->values.tls.ca_cert_path = NULL;
    if (entr.type == DICTIONARY_STRING) {
        cfg->values.tls.ca_cert_path = malloc(MAX_DICTIONARY_STRING_LENGTH_BYTES);
        strncpy(cfg->values.tls.ca_cert_path, entr.value.string.c, MAX_DICTIONARY_STRING_LENGTH_BYTES);
        cfg->values.tls.ca_cert_path[MAX_DICTIONARY_STRING_LENGTH_BYTES - 1] = '\0';
    }

    entr = config_get(cfg, "RASTA_CERT_PATH");
    cfg->values.tls.cert_path = NULL;
    if (entr.type == DICTIONARY_STRING) {
        cfg->values.tls.cert_path = malloc(MAX_DICTIONARY_STRING_LENGTH_BYTES);
        strncpy(cfg->values.tls.cert_path, entr.value.string.c, MAX_DICTIONARY_STRING_LENGTH_BYTES);
        cfg->values.tls.cert_path[MAX_DICTIONARY_STRING_LENGTH_BYTES - 1] = '\0';
    }

    entr = config_get(cfg, "RASTA_KEY_PATH");
    cfg->values.tls.key_path = NULL;
    if (entr.type == DICTIONARY_STRING) {
        cfg->values.tls.key_path = malloc(MAX_DICTIONARY_STRING_LENGTH_BYTES);
        strncpy(cfg->values.tls.key_path, entr.value.string.c, MAX_DICTIONARY_STRING_LENGTH_BYTES);
        cfg->values.tls.key_path[MAX_DICTIONARY_STRING_LENGTH_BYTES - 1] = '\0';
    }

#ifdef ENABLE_TLS
    entr = config_get(cfg, "RASTA_TLS_HOSTNAME");
    if (entr.type == DICTIONARY_STRING) {
        cfg->values.tls.tls_hostname[MAX_DOMAIN_LENGTH - 1] = 0;
        memcpy(cfg->values.tls.tls_hostname, entr.value.string.c, MAX_DOMAIN_LENGTH);
    }
    entr = config_get(cfg, "RASTA_TLS_PEER_CERT_PATH");
    cfg->values.tls.peer_tls_cert_path = NULL;
    if (entr.type == DICTIONARY_STRING) {
        cfg->values.tls.peer_tls_cert_path = malloc(MAX_DICTIONARY_STRING_LENGTH_BYTES);
        strncpy(cfg->values.tls.peer_tls_cert_path, entr.value.string.c, MAX_DICTIONARY_STRING_LENGTH_BYTES);
        cfg->values.tls.peer_tls_cert_path[MAX_DICTIONARY_STRING_LENGTH_BYTES - 1] = '\0';
    }
#endif

    cfg->values.kex.mode = KEY_EXCHANGE_MODE_NONE;

#ifdef ENABLE_OPAQUE
    entr = config_get(cfg, "RASTA_KEX_MODE");
    if (entr.type == DICTIONARY_STRING) {
        bool accepted = false;
        if (!strncmp(entr.value.string.c, stringify(KEY_EXCHANGE_MODE_OPAQUE),
                     strlen(stringify(KEY_EXCHANGE_MODE_OPAQUE)))) {
            cfg->values.kex.mode = KEY_EXCHANGE_MODE_OPAQUE;
            accepted = true;
        }
        if (!accepted) {
            fprintf(stderr, "Unknown or unsupported KEX mode: %s\n", entr.value.string.c);
            abort();
        }
    }
    entr = config_get(cfg, "RASTA_KEX_PSK");

    if (entr.type == DICTIONARY_STRING) {
        cfg->values.kex.psk[KEX_PSK_MAX - 1] = 0;
        memcpy(cfg->values.kex.psk, entr.value.string.c, KEX_PSK_MAX);
    }

    entr = config_get(cfg, "RASTA_KEX_REKEYING_INTERVAL_MS");
    // default = no rekeying
    cfg->values.kex.rekeying_interval_ms = 0;
    if (entr.type == DICTIONARY_NUMBER) {
        cfg->values.kex.rekeying_interval_ms = entr.value.number;
    }
    cfg->values.kex.has_psk_record = false;
    entr = config_get(cfg, "RASTA_KEX_PSK_RECORD");
    if (entr.type == DICTIONARY_STRING) {
        const size_t record_header_length = strlen(CONFIGURATION_FILE_USER_RECORD_HEADER) + 1;
        const size_t given_record_length = strlen(entr.value.string.c);
        // -1 for null byte, 2 * the target size since 0-padded hex format instead of binary (i.e. 2 chars per byte)
        const size_t expected_record_length = record_header_length - 1 + 2 * sizeof(cfg->values.kex.psk_record);
        char record_header[record_header_length];
        size_t output_start = 0;
        memcpy(record_header, entr.value.string.c, record_header_length - 1);
        record_header[record_header_length - 1] = 0;

        if (given_record_length != expected_record_length) {
            fprintf(stderr, "Invalid PSK record length %lu bytes - %lu bytes were expected!\n", given_record_length, expected_record_length);
        }

        if (strncmp(record_header, CONFIGURATION_FILE_USER_RECORD_HEADER, record_header_length - 1) != 0) {
            fprintf(stderr, "Unknown psk record header: %s\n", record_header);
            abort();
        }
        cfg->values.kex.has_psk_record = true;

        for (size_t i = record_header_length - 1; i < given_record_length; i += 2) {
            sscanf((char *)&entr.value.string.c[i], "%02" SCNx8, (unsigned char *)&cfg->values.kex.psk_record[output_start++]);
        }
    }

#endif
}

/*
 * Public functions
 */
int config_load(struct RastaConfig *config, const char *filename) {

    memset(config, 0, sizeof(struct RastaConfig));

    FILE *f;
    char buf[CONFIG_BUFFER_LENGTH];
    strcpy(config->filename, filename);

    logger_init(&config->logger, LOG_LEVEL_INFO, LOGGER_TYPE_CONSOLE);

    f = fopen(config->filename, "r");
    if (!f) {
        logger_log(&config->logger, LOG_LEVEL_ERROR, config->filename, "File not found");
        return 1;
    }

    config->dictionary = dictionary_create(2);

    int n = 1;
    while (fgets(buf, CONFIG_BUFFER_LENGTH, f) != NULL) {
        // initialize parser
        struct LineParser p;
        parser_init(&p, buf, n, config);

        // skip empty start
        parser_skipBlank(&p);

        // ignore comments
        if (p.current == ';') {
            n++;
            continue;
        }

        // ignore empty lines
        if (p.pos + 1 >= strlen(buf)) {
            n++;
            continue;
        }

        // ignore lines starting with unexpected characters
        if (!(isdigit(p.current) || isalpha(p.current) || (p.current == '_'))) {
            n++;
            continue;
        }

        // parse key
        char key[MAX_DICTIONARY_STRING_LENGTH_BYTES];
        parser_parseIdentifier(&p, key);

        // skip empty start
        parser_skipBlank(&p);

        if (p.current != '=') {
            logger_log(&p.cfg->logger, LOG_LEVEL_ERROR, p.cfg->filename, "Error in line %d: Expected '=' but found '%c'", p.line, p.current);
            n++;
            continue;
        }
        parser_next(&p);

        // skip empty start
        parser_skipBlank(&p);

        parser_parseValue(&p, key);

        n++;
    }

    fclose(f);

    // initialize standard value
    config_setstd(config);

    return 0;
}

struct DictionaryEntry config_get(struct RastaConfig *cfg, const char *key) {
    return dictionary_get(&cfg->dictionary, key);
}

void config_free(struct RastaConfig *cfg) {
    dictionary_free(&cfg->dictionary);
    if (cfg->values.redundancy.connections.count > 0) free(cfg->values.redundancy.connections.data);
    if (cfg->values.tls.ca_cert_path != NULL) free(cfg->values.tls.ca_cert_path);
    if (cfg->values.tls.cert_path != NULL) free(cfg->values.tls.cert_path);
    if (cfg->values.tls.key_path != NULL) free(cfg->values.tls.key_path);
    if (cfg->values.tls.peer_tls_cert_path != NULL) free(cfg->values.tls.peer_tls_cert_path);
}

unsigned long mix(unsigned long a, unsigned long b, unsigned long c) {
    a = a - b;
    a = a - c;
    a = a ^ (c >> 13);
    b = b - c;
    b = b - a;
    b = b ^ (a << 8);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 13);
    a = a - b;
    a = a - c;
    a = a ^ (c >> 12);
    b = b - c;
    b = b - a;
    b = b ^ (a << 16);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 5);
    a = a - b;
    a = a - c;
    a = a ^ (c >> 3);
    b = b - c;
    b = b - a;
    b = b ^ (a << 10);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 15);
    return c;
}

/**
 * generate a 4 byte random number
 * @return 4 byte random number
 */
uint32_t long_random(void) {
    srand(mix(clock(), time(NULL), getpid()));
    uint32_t r = 0;

    for (int i = 0; i < 32; i++) {
        r = r * 2 + rand() % 2;
    }
    return r;
}

/**
 * Gets the initial sequence number from the config. If the sequence number was set to a negative value, a random number will
 * be used
 * @param config the config that is used to get the sequence number
 * @return the initial sequence number
 */
uint32_t get_initial_seq_num(struct RastaConfig *config) {
    struct DictionaryEntry init_seq = config_get(config, RASTA_CONFIG_KEY_INITIAL_SEQ_NUM);

    // return specified value if > 0, random number if < 0 or not in config
    return (init_seq.type == DICTIONARY_NUMBER && init_seq.value.number >= 0) ? init_seq.value.unumber : long_random();
}

void load_configfile(rasta_config_info *c, struct logger_t *logger, const char *config_file_path) {
    struct RastaConfig config;
    config_load(&config, config_file_path);
    *c = config.values;

    // load logger configuration
    struct DictionaryEntry logger_ty = config_get(&config, RASTA_CONFIG_KEY_LOGGER_TYPE);
    struct DictionaryEntry logger_maxlvl = config_get(&config, RASTA_CONFIG_KEY_LOGGER_MAX_LEVEL);
    struct DictionaryEntry logger_file = config_get(&config, RASTA_CONFIG_KEY_LOGGER_FILE);

    if (logger_ty.type == DICTIONARY_NUMBER && logger_maxlvl.type == DICTIONARY_NUMBER) {
        logger_init(logger, (log_level)logger_maxlvl.value.number, (logger_type)logger_ty.value.number);

        if (logger->type == LOGGER_TYPE_FILE) {
            // need to set log file
            if (logger_file.type == DICTIONARY_STRING) {
                logger_set_log_file(logger, logger_file.value.string.c);
            } else {
                // error in config
                abort();
            }
        }
    } else {
        // error in config
        abort();
    }

    // get accepted versions from config
    struct DictionaryEntry config_accepted_version = config_get(&config, RASTA_CONFIG_KEY_ACCEPTED_VERSIONS);

    if (config_accepted_version.type == DICTIONARY_ARRAY) {
        c->accepted_version_count = config_accepted_version.value.array.count;
        c->accepted_versions = malloc(c->accepted_version_count * 5 * sizeof(char));
        for (unsigned int i = 0; i < c->accepted_version_count; ++i) {
            logger_log(logger, LOG_LEVEL_DEBUG, "RaSTA HANDLE_INIT", "Loaded accepted version: %s", config_accepted_version.value.array.data[i].c);
            memcpy(c->accepted_versions[i], config_accepted_version.value.array.data[i].c, 4);
            c->accepted_versions[i][4] = '\0';
        }
    }

    c->initial_sequence_number = get_initial_seq_num(&config);

    dictionary_free(&config.dictionary);
}
