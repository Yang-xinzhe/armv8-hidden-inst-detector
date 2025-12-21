#include "fs_utils.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/*
 * mkdir -p implementation.
 * - creates intermediate components
 * - treats EEXIST as success if the component is already a directory
 */
int mkdir_p(const char *path, mode_t mode)
{
    if (!path || path[0] == '\0') return -1;

    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;

    /* Copy and normalize: strip trailing '/' */
    memcpy(tmp, path, len + 1);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }

    /* Walk components */
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (mkdir(tmp, mode) != 0) {
                if (errno != EEXIST) return -1;
                struct stat st;
                if (stat(tmp, &st) != 0) return -1;
                if (!S_ISDIR(st.st_mode)) return -1;
            }
            tmp[i] = '/';
        }
    }

    if (mkdir(tmp, mode) != 0) {
        if (errno != EEXIST) return -1;
        struct stat st;
        if (stat(tmp, &st) != 0) return -1;
        if (!S_ISDIR(st.st_mode)) return -1;
    }
    return 0;
}


