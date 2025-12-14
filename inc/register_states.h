#pragma once
#define _GNU_SOURCE
#include <stdint.h>

typedef struct __attribute__((aligned(4))) {
    uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
    uint32_t cpsr;
} RegisterStates;

typedef struct __attribute__((aligned(16))) {
    uint8_t q[16][16]; // 16 registers * 16 bytes
    uint32_t fpscr;    // Floating-Point Status and Control Register
    uint8_t _padding[12]; // Padding to align to 16 bytes (Total 272 bytes)
} SimdRegisterStates;

extern RegisterStates *reg_state_base_slot;
extern SimdRegisterStates *simd_state_base_slot;