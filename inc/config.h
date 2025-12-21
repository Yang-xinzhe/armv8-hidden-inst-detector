#pragma once

#include <stddef.h>

/*
 * Lightweight key=value config loader (no third-party deps).
 *
 * Precedence (recommended by callers):
 *   CLI args > config file > defaults
 */

typedef struct {
    /* Phase 1 (screening) */
    char phase1_input_dir[256];    /* e.g., results_A32 */
    char phase1_output_dir[256];   /* e.g., bitmap_results */
    int  phase1_max_files;         /* e.g., 256 */
    int  phase1_timeout_seconds;   /* e.g., 7200 */

    /* Phase 2 (sandbox analysis / fuzzers) */
    char phase2_input_dir[256];    /* e.g., hidden_insn */
    char phase2_output_dir[256];   /* base dir, e.g., res/phase2 (empty means current dir) */

    /* Generic */
    int loaded;                    /* 1 if a config file was loaded */
} ProjectConfig;

/* Initialize cfg with built-in defaults. */
void project_config_init(ProjectConfig *cfg);

/*
 * Load config from a text file. Unknown keys are ignored.
 * Returns:
 *   0  on success
 *  -1  on error (file not found / parse failure)
 */
int project_config_load(ProjectConfig *cfg, const char *path);



