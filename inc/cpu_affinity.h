#pragma once
#define _GNU_SOURCE
#include <sched.h>
#include <sys/mman.h>
#include <stdio.h>

int set_cpu_affinity(pid_t pid, int core_id);