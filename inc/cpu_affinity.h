#pragma once
#include <sched.h>


int set_cpu_affinity(pid_t pid, int core_id);