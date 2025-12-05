#include "pmu_counter.h"
#include "sandbox.h"

int perf_event_open(struct perf_event_attr *hw_event,
    pid_t pid, int cpu, int group_fd, unsigned long flags) {
return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int init_memory_monitor(PmuCounter *pmu) {
    struct perf_event_attr attr = {0};
    attr.type = 8;
    attr.size = sizeof(attr);
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.pinned = 1;
    attr.exclusive = 1;
    
    attr.config = 0x0006;
    pmu->ld_retired_fd = perf_event_open(&attr, 0, 1, -1, 0);
    if (pmu->ld_retired_fd == -1) {
        printf("LD_RETIRED failed: %s\n", strerror(errno));
    }

    attr.config = 0x0007;
    pmu->st_retired_fd = perf_event_open(&attr, 0, 1, -1, 0);
    if (pmu->st_retired_fd == -1) {
        printf("ST_RETIRED failed: %s\n", strerror(errno));
    }
    
    return 0;
}