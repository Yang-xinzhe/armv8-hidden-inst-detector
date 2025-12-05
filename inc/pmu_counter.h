#pragma once
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/shm.h>
#include <sys/mman.h> 
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>

typedef struct {
    int ld_retired_fd;
    int st_retired_fd;
} PmuCounter;

typedef struct {
    uint64_t ld_count;
    uint64_t st_count;
} PmuResult;

int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags);
int init_memory_monitor(PmuCounter *pmu);
