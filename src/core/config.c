#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rstrip(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || isspace((unsigned char)s[n - 1]))) {
        s[n - 1] = '\0';
        n--;
    }
}

static char *lskip(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int streq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static void set_str(char dst[256], const char *src)
{
    if (!src) return;
    snprintf(dst, 256, "%s", src);
}

static int parse_int(const char *s, int *out)
{
    if (!s || !out) return -1;
    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (errno != 0) return -1;
    if (end == s) return -1;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return -1;
    if (v < -2147483648L || v > 2147483647L) return -1;
    *out = (int)v;
    return 0;
}

void project_config_init(ProjectConfig *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    /* Built-in defaults (keep compatible with current codebase) */
    set_str(cfg->phase1_input_dir, "");
    set_str(cfg->phase1_output_dir, "bitmap_results");
    cfg->phase1_max_files = 256;
    cfg->phase1_timeout_seconds = 7200;
    cfg->loaded = 0;
}

int project_config_load(ProjectConfig *cfg, const char *path)
{
    if (!cfg || !path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f) != NULL) {
        rstrip(line);
        char *p = lskip(line);
        if (*p == '\0') continue;
        if (*p == '#' || *p == ';') continue;

        /* Strip inline comments: "a=b # c" */
        for (char *c = p; *c; c++) {
            if (*c == '#') {
                *c = '\0';
                break;
            }
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = lskip(p);
        rstrip(key);
        char *val = lskip(eq + 1);
        rstrip(val);

        if (streq(key, "phase1_input_dir")) {
            set_str(cfg->phase1_input_dir, val);
        } else if (streq(key, "phase1_output_dir")) {
            set_str(cfg->phase1_output_dir, val);
        } else if (streq(key, "phase1_max_files")) {
            (void)parse_int(val, &cfg->phase1_max_files);
        } else if (streq(key, "phase1_timeout_seconds")) {
            (void)parse_int(val, &cfg->phase1_timeout_seconds);
        } else {
            /* Unknown keys are ignored for forward compatibility */
        }
    }

    fclose(f);
    cfg->loaded = 1;
    return 0;
}



