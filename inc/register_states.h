#pragma once
#define _GNU_SOURCE
#include <stdint.h>

typedef struct __attribute__((aligned(4))) {
    uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
    uint32_t sp, lr, pc;
    uint32_t cpsr;
} RegisterStates;
