#include "cpu_affinity.h"

int set_cpu_affinity(pid_t pid, int core_id) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);

    if (sched_setaffinity(pid, sizeof(mask), &mask) < 0) {
        perror("Setting CPU affinity failed");
        return -1;
    }

    mlockall(MCL_CURRENT | MCL_FUTURE);

    return 0;
}