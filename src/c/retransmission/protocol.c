#include "protocol.h"

#include <time.h>
#include <stdlib.h>
#include <rasta/config.h>

uint64_t get_current_time_ms() {
    uint64_t current_time;
    struct timespec current_time_tv;
    clock_gettime(CLOCK_MONOTONIC, &current_time_tv);

    current_time = current_time_tv.tv_nsec / NS_PER_MS + current_time_tv.tv_sec * MS_PER_S;
    return current_time;
}

/**
 * Converts a unsigned long into a uchar array
 * @param v the uchar array
 * @param result the assigned uchar array; length should be 4
 */
void longToBytes2(unsigned long v, unsigned char *result) {
    result[0] = (unsigned char)(v >> 24 & 0xFF);
    result[1] = (unsigned char)(v >> 16 & 0xFF);
    result[2] = (unsigned char)(v >> 8 & 0xFF);
    result[3] = (unsigned char)(v & 0xFF);
}

/**
 * Converts a uchar array to a ulong
 * @param v the uchar array
 * @return the ulong
 */
uint32_t bytesToLong2(const unsigned char v[4]) {
    uint32_t result = 0;
    result = (v[0] << 24) + (v[1] << 16) + (v[2] << 8) + v[3];
    return result;
}

/**
 * compares two RaSTA protocol version
 * @param local_version the local version
 * @param remote_version the remote version
 * @return  0 if local_version == remote_version
 *         -1 if local_version < remove_version
 *          1 if local_version > remote_version
 */
int compare_version(const char (*local_version)[5], const char (*remote_version)[5]) {
    char *tmp;
    long local = strtol(*local_version, &tmp, 4);
    long remote = strtol(*remote_version, &tmp, 4);

    if (local == remote) {
        return 0;
    } else if (local < remote) {
        return -1;
    } else {
        return 1;
    }
}

/**
 * checks if the given RaSTA protocol version is accepted
 * @param version the version of the remote
 * @return 1 if the remote version is accepted, else 0
 */
int version_accepted(rasta_config_info *config, const char (*version)[5]) {
    for (unsigned int i = 0; i < config->accepted_version_count; ++i) {
        // const (char[5]) * av = ;
        if (compare_version(&config->accepted_versions[i], version) == 0) {
            // match, version is in accepted version list
            return 1;
        }
    }

    // otherwise (something with config went wrong or version was not in accepted versions) return 0
    return 0;
}
