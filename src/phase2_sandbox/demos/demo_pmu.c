#include "pmu_counter.h"
#include "sandbox.h"

#include <unistd.h>
#include <fcntl.h>

void ensure_pmu_enabled() {
    int fd = open("/proc/pmu_user_enable", O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "1", 1) < 0) {
            // ignore error
        }
        close(fd);
    }
}

int main(void)
{
    uint32_t test_ld_st_insns[] = {
    // ---------- LDR 系列 ----------
    0xE7900001,   // LDR r0, [r0, r1]
    0xE7911002,   // LDR r1, [r1, r2]
    0xE7922003,   // LDR r2, [r2, r3]
    0xE7933004,   // LDR r3, [r3, r4]
    0xE7944005,   // LDR r4, [r4, r5]
    0xE7955006,   // LDR r5, [r5, r6]
    0xE7966007,   // LDR r6, [r6, r7]
    0xE7977008,   // LDR r7, [r7, r8]
    0xE7988009,   // LDR r8, [r8, r9]
    0xE799900A,   // LDR r9, [r9, r10]

    // ---------- STR 系列 ----------
    0xE7800001,   // STR r0, [r0, r1]
    0xE7811002,   // STR r1, [r1, r2]
    0xE7822003,   // STR r2, [r2, r3]
    0xE7833004,   // STR r3, [r3, r4]
    0xE7844005,   // STR r4, [r4, r5]
    0xE7855006,   // STR r5, [r5, r6]
    0xE7866007,   // STR r6, [r6, r7]
    0xE7877008,   // STR r7, [r7, r8]
    0xE7888009,   // STR r8, [r8, r9]
    0xE789900A,   // STR r9, [r9, r10]
};
    int len = sizeof(test_ld_st_insns)/sizeof(test_ld_st_insns[0]);

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

    PmuResult res = {0};

    ensure_pmu_enabled();

    pmu_init();

    uint8_t insn_bytes[4];
    for(int i = 0 ; i < len ; i++) {
       size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), test_ld_st_insns[i]);
        execute_insn_page_pmu(insn_bytes, buf_length, &res); 
        printf("Measured: LD_RETIRED = %u, ST_RETIRED = %u\n", res.ld_result, res.st_result);
    }
    


    

    return 0;
}
