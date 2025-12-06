#include "core.h"
#include "sandbox.h"
#include "pmu_counter.h"

PmuCounter pmu = {0};

int main() {
    uint32_t hidden_instruction = 0xE5900000;

    RegisterStates *states = malloc(2 * sizeof(RegisterStates));
    if (!states) {
        perror("malloc");
        return 1;
    }

    PmuResult *result = malloc(sizeof(PmuResult));
    if(!result) {
        perror("malloc2");
        return 1;
    }

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

    if(init_memory_monitor(&pmu) != 0) {
        printf("PMU initial failed!\n");
    }
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes,sizeof(insn_bytes), hidden_instruction);

    execute_insn_page_pmu(insn_bytes, buf_length, states, &pmu, result);

    printf("Load Count: %llu\nStore Count: %llu\n", result->ld_count, result->st_count);
    return 0;
}