#pragma once

#include "rastablake2.h"
#include "rastamd4.h"
#include "rastasiphash24.h"
#include "rastautil.h"

typedef struct rasta_hashing_ctx {
    /**
     * The hashing algorithm
     */
    rasta_hash_algorithm algorithm;
    /**
     * The length of the resulting hash
     */
    rasta_checksum_type hash_length;
    /**
     * The key / iv for the hashing algorithm
     */
    struct RastaByteArray key;
} rasta_hashing_context_t;

/**
 * Calculates a checksum over the given data using the parameters in the hashing context
 * @param data the data to hash
 * @param context the hashing context that contains the neccessary parameters for hashing the data
 * @param hash the resulting hash
 */
void rasta_calculate_hash(struct RastaByteArray data, rasta_hashing_context_t *context, unsigned char *hash);

/**
 * Sets the key of the hashing context based the the MD4 initial value
 * @param context the context where the key is set
 * @param a A part of the initial MD4 value
 * @param b B part of the initial MD4 value
 * @param c C part of the initial MD4 value
 * @param d D part of the initial MD4 value
 */
void rasta_md4_set_key(rasta_hashing_context_t *context, MD4_u32plus a, MD4_u32plus b, MD4_u32plus c, MD4_u32plus d);
/**
 * Sets a variable-length key for use with the different hash functions
 * @param context context for the key
 * @param key arbitrary, random bytes
 * @param key_length number of bytes
 */
void rasta_set_hash_key_variable(rasta_hashing_context_t *context, const char *key, size_t key_length);
