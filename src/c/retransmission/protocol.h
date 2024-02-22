#pragma once

#include <stdint.h>

#include <rasta/config.h>

/**
 * the RaSTA version that is implemented
 */
#define RASTA_VERSION "0303"

uint64_t get_current_time_ms();

int compare_version(char (*local_version)[5], char (*remote_version)[5]);
int version_accepted(rasta_config_info *config, char (*version)[5]);
