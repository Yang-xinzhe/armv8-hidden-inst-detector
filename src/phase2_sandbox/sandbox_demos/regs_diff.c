#include "core.h"
#include "sandbox.h"
#include "register_states.h"

void print_regs(RegisterStates *r, const char *prefix) {
    printf("=== %s ===\n", prefix);
    printf("R0: %08x R1: %08x R2: %08x R3: %08x\n", r->r0, r->r1, r->r2, r->r3);
    printf("R4: %08x R5: %08x R6: %08x R7: %08x\n", r->r4, r->r5, r->r6, r->r7);
    printf("R8: %08x R9: %08x R10:%08x R11:%08x\n", r->r8, r->r9, r->r10, r->r11);
    printf("R12:%08x SP: %08x LR: %08x PC: %08x\n", r->r12, r->sp, r->lr, r->pc);
    printf("CPSR: %08x\n", r->cpsr);
}

int main() {
    uint32_t hidden_instruction = 0xE0800001;

    RegisterStates *states = malloc(2 * sizeof(RegisterStates));
    if (!states) {
        perror("malloc");
        return 1;
    }

    memset(states, 0, sizeof(RegisterStates) * 2);

    init_signal_handler(signal_handler, SIGILL,    SA_NONE);
    init_signal_handler(signal_handler, SIGSEGV,   SA_NONE);
    init_signal_handler(signal_handler, SIGTRAP,   SA_NONE);
    init_signal_handler(signal_handler, SIGBUS,    SA_NONE);

    init_signal_handler(signal_handler, SIGRTMIN,  SA_NODEFER);
    init_signal_handler(signal_handler, SIGVTALRM, SA_NODEFER);


    if (init_watchdog_timer() != 0) {
        fprintf(stderr, "Failed to initialize watchdog timer\n");
        return 1;
    }

    if (init_insn_page() != 0) {
        perror("init_insn_page");
        return 1;
    }

    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), hidden_instruction);
    execute_insn_page(insn_bytes, buf_length, states);

    print_regs(&states[0], "Before Execution");
    print_regs(&states[1], "After Execution");

    return 0;
}