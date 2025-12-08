#pragma once
#define _GNU_SOURCE
#include "core.h"

typedef struct {
    int32_t ld_result;
    int32_t st_result;
} PmuResult;

void pmu_init(void);