#pragma once
#define _GNU_SOURCE
#include "core.h"
#include "sandbox.h"
#include "register_states.h"

typedef struct {
    int ld_retired_fd;
    int st_retired_fd;
} PmuCounter;

typedef struct {
    RegisterStates *states;
    PmuCounter     *pmu;
} PmuExecContext;

typedef struct {
    uint64_t ld_count;
    uint64_t st_count;
} PmuResult;

int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags);
int init_memory_monitor(PmuCounter *pmu);
void pre_pmu(void *ctx);
void post_pmu(void *ctx);
void execute_insn_page_pmu(uint8_t *insn_bytes, size_t insn_length, RegisterStates *states, PmuCounter *pmu, PmuResult *result);