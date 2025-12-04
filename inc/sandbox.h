#pragma once
#include "core.h"
#include "register_states.h"

#define PAGE_SIZE   4096
#define MY_SIGSTKSZ 8192
#define SA_NONE     0

extern void *insn_region;                  // [guard][code][guard]
extern void *insn_page;                    // Executable Page
extern volatile sig_atomic_t last_insn_signum;
extern volatile sig_atomic_t executing_insn;
extern volatile sig_atomic_t timeout_occurred;
extern uint32_t insn_offset;
extern uint32_t mask;

extern char boilerplate_start, boilerplate_end, insn_location;

extern sigjmp_buf escape_env;
extern timer_t watchdog_timer;

extern uint8_t sig_stack_array[MY_SIGSTKSZ];
extern stack_t sig_stack;

void signal_handler(int, siginfo_t *, void*);
void init_signal_handler(void(*handler)(int, siginfo_t*, void*), int, int);

int init_insn_page(void);
void execute_insn_page(uint8_t*, size_t, RegisterStates *states);
size_t fill_insn_buffer(uint8_t*, size_t, uint32_t);

int init_watchdog_timer(void);
void arm_watchdog_us(int us);
void disarm_watchdog(void);