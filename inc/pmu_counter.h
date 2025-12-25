#pragma once
#define _GNU_SOURCE
#include "core.h"

typedef struct {
    int32_t ld_result;  // ARM DDI 0487G.b Page K3-8454 0x0070-LD_SPEC
    int32_t st_result;  // ARM DDI 0487G.b Page K3-8455 0x0071-ST_SPEC
} PmuResult;

void pmu_init(void);