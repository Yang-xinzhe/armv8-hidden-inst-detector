#pragma once

#include <sys/types.h>

/*
 * Small filesystem helpers (no third-party deps).
 */

/*
 * Recursively create a directory path (like `mkdir -p`).
 * Returns 0 on success, -1 on error.
 */
int mkdir_p(const char *path, mode_t mode);


