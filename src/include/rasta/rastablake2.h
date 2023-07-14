#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * generates a BLAKE2 (rasta_blake2b) hash for the given data data and saves it in result
 * @param data array of the data
 * @param data_length length of data
 * @param key the key for the BLAKE2 function
 * @param key_length the length of the key
 * @param hash_type type of security code (0 means no code, 1 means first 8 bytes, 2 means first 16 bytes)
 * @param result array for the result
 */
void generateBlake2(unsigned char *data, int data_length, const unsigned char *key, int key_length, int hash_type, unsigned char *result);

int rasta_blake2b_selftest();

/*
 * Start of BLAKE2 implementation from RFC 7693
 * https://tools.ietf.org/html/rfc7693#page-16
 */

/**
 * state context
 */
typedef struct {
    /**
     * input buffer
    */
    uint8_t b[128];
    /**
     * chained state
    */
    uint64_t h[8];
    /**
     * total number of bytes
    */
    uint64_t t[2];
    /**
     * pointer for b[]
    */
    size_t c;
    /**
     * digest size
    */
    size_t outlen;
} rasta_blake2b_ctx;

/**
 * Initialize the hashing context @p ctx with optional key @p key.
 * @param ctx the hashing context
 * @param outlen the digest size in bytes (1 <= outlen <= 64 ).
 * @param key secret key (also <= 64 bytes), is optional (keylen = 0).
 * @param keylen the length of the key
*/
int rasta_blake2b_init(rasta_blake2b_ctx *ctx, size_t outlen,
                       const void *key, size_t keylen);

/**
 * Add @p inlen bytes from @p in into the hash.
 * @param ctx hashing context
 * @param in data to be hashed
 * @param inlen the length of the data
*/
void rasta_blake2b_update(rasta_blake2b_ctx *ctx,
                          const void *in, size_t inlen);

/**
 * Generate the message digest (size given in init).
 * @param ctx hashing context
 * @param out pointer into which to place the digest
*/
void rasta_blake2b_final(rasta_blake2b_ctx *ctx, void *out);

/**
 * All-in-one convenience function.
 * @param out return buffer for digest
 * @param outlen the length of @p out
 * @param key optional secret key
 * @param keylen the length of @p key
 * @param in data to be hashed
 * @param inlen the length of @p in
*/
int rasta_blake2b(void *out, size_t outlen,
                  const void *key, size_t keylen,
                  const void *in, size_t inlen);
