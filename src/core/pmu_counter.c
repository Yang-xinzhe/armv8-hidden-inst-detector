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
    attr.exclusive = 0;
    
    pid_t tid = getpid();
    int cpu = -1;

    attr.config = 0x0006;
    pmu->ld_retired_fd = perf_event_open(&attr, tid, cpu, -1, 0);
    if (pmu->ld_retired_fd == -1) {
        printf("LD_RETIRED failed: %s\n", strerror(errno));
    }

    attr.config = 0x0007;
    pmu->st_retired_fd = perf_event_open(&attr, tid, cpu, -1, 0);
    if (pmu->st_retired_fd == -1) {
        printf("ST_RETIRED failed: %s\n", strerror(errno));
    }
    
    return 0;
}

void pre_pmu(void *ctx) {
    PmuExecContext *c = (PmuExecContext *)ctx;
    PmuCounter *pmu = c->pmu;
    ioctl(pmu->ld_retired_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(pmu->st_retired_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(pmu->ld_retired_fd, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(pmu->st_retired_fd, PERF_EVENT_IOC_ENABLE, 0);
}

void post_pmu(void *ctx) {
    PmuExecContext *c = (PmuExecContext *)ctx;
    PmuCounter *pmu = c->pmu;
    ioctl(pmu->ld_retired_fd, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(pmu->st_retired_fd, PERF_EVENT_IOC_DISABLE, 0);
}

static void exec_pmu(void *addr, void *ctx) {
    PmuExecContext *c = (PmuExecContext *)ctx;
    RegisterStates *states = c->states;
    void (*exec_page)(RegisterStates*) = (void (*)(RegisterStates*))addr;
    exec_page(states);
}

void execute_insn_page_pmu(uint8_t *insn_bytes, size_t insn_length, RegisterStates *states, PmuCounter *pmu, PmuResult *result) {
    PmuExecContext ctx = {
        .states = states,
        .pmu    = pmu,
    };
    execute_insn_page(insn_bytes, insn_length, &ctx, pre_pmu, exec_pmu, post_pmu);

    ssize_t __attribute__((unused)) n;
    if (result) {
        n = read(pmu->ld_retired_fd, &result->ld_count, sizeof(uint64_t));
        n = read(pmu->st_retired_fd, &result->st_count, sizeof(uint64_t));
    }
}