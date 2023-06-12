#pragma once

#include <stdint.h>
#include <rasta/config.h>

/**
 * the RaSTA version that is implemented
 */
#define RASTA_VERSION "0303"

#define NS_PER_SEC 1000000000
#define MS_PER_S 1000
#define NS_PER_MS 1000000

uint64_t get_current_time_ms();

int compare_version(const char local_version[5], const char remote_version[5]);
int version_accepted(rasta_config_info *config, const char version[5]);
