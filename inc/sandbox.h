#pragma once
#include "core.h"

#define PAGE_SIZE 4096

void *insn_region = NULL;                  // [guard][code][guard]
void *insn_page = NULL;                    // Executable Page
volatile sig_atomic_t last_insn_signum = 0;
volatile sig_atomic_t executing_insn = 0;
volatile sig_atomic_t timeout_occurred = 0;
uint32_t insn_offset = 0;
uint32_t mask = 0x1111;

extern sigjmp_buf escape_env;
extern timer_t watchdog_timer;

extern uint8_t sig_stack_array[SIGSTKSZ];
stack_t sig_stack = {
    .ss_size = SIGSTKSZ,
    .ss_sp = sig_stack_array,
};

void signal_handler(int, siginfo_t *, void*);
void init_signal_handler(void(*handler)(int, siginfo_t*, void*), int, int);

inline void arm_watchdog_us(int us);
inline void disarm_watchdog(void);