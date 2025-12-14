#include "core.h"
#include "sandbox.h"
#include "register_states.h"

// 定义 SIMD 寄存器状态结构体
// AArch32 模式下有 16 个 128 位寄存器 (Q0-Q15)
// 它们覆盖了 D0-D31 和 S0-S31


SimdRegisterStates *simd_state_base_slot = NULL;

int init_simd_state_slot(void)
{
    simd_state_base_slot = aligned_alloc(16, 2 * sizeof(SimdRegisterStates));
    if (!simd_state_base_slot) {
        perror("aligned_alloc");
        return -1;
    }
    memset(simd_state_base_slot, 0, 2 * sizeof(SimdRegisterStates));
    return 0;
}

void print_simd_diff(const SimdRegisterStates *before, const SimdRegisterStates *after)
{
    printf("=== SIMD Register Differences ===\n");
    int changed = 0;

    for (int i = 0; i < 16; i++) {
        if (memcmp(before->q[i], after->q[i], 16) != 0) {
            printf("Q%d changed:\n", i);
            printf("  Before: ");
            for (int j = 15; j >= 0; j--) printf("%02x", before->q[i][j]);
            printf("\n  After : ");
            for (int j = 15; j >= 0; j--) printf("%02x", after->q[i][j]);
            printf("\n");
            changed = 1;
        }
    }

    if (before->fpscr != after->fpscr) {
        printf("FPSCR changed: %08x -> %08x\n", before->fpscr, after->fpscr);
        changed = 1;
    }

    if (!changed) {
        printf("No visible changes in SIMD registers or FPSCR.\n");
    }
}

int main() {

    uint32_t test_simd_insns[] = {
        0xF2220844, // VADD.I32 Q0, Q1, Q2  (Q0 = Q1(0x01..) + Q2(0x02..) = 0x03..)
        // --- 低位寄存器组 (Q0 - Q7) ---
        0xF2220844, // Q0:  VADD.I32 Q0, Q1, Q2   (整数加法)
        0xF2642846, // Q1:  VSUB.I32 Q1, Q2, Q3   (整数减法)
        0xF2264958, // Q2:  VMUL.I32 Q2, Q3, Q4   (整数乘法)
        0xF208615A, // Q3:  VAND     Q3, Q4, Q5   (位与)
        0xF22A815C, // Q4:  VORR     Q4, Q5, Q6   (位或)
        0xF30C515E, // Q5:  VEOR     Q5, Q6, Q7   (位异或)
        0xF22E6640, // Q6:  VMAX.S32 Q6, Q7, Q8   (取最大值, Q8=D16)
        0xF2207652, // Q7:  VMIN.S32 Q7, Q8, Q1   (取最小值)

        // --- 高位寄存器组 (Q8 - Q15) ---
        // 注意：从 Q8 开始，目标寄存器的编码中 "D" 位 (bit 22) 会被置为 1
        
        0xF2020D64, // Q8:  VADD.F32 Q8, Q1, Q2   (浮点加法: D16 = D2 + D4)
        0xF2242D66, // Q9:  VSUB.F32 Q9, Q2, Q3   (浮点减法: D18 = D4 - D6)
        0xF3064D68, // Q10: VMUL.F32 Q10, Q3, Q4  (浮点乘法: D20 = D6 * D8)
        0xF2106172, // Q11: VBIC     Q11, Q0, Q1  (位清除: Q11 = Q0 AND NOT Q1)
        0xF3008870, // Q12: VCEQ.I32 Q12, Q0, Q0  (比较相等: Q12应全为1, 因为Q0==Q0)
        0xF222A364, // Q13: VCGT.S32 Q13, Q1, Q2  (比较大于: Q13 = (Q1 > Q2))
        0xF332C544, // Q14: VMVN     Q14, Q2      (按位取反移动: Q14 = ~Q2)
        0xF280E070, // Q15: VMOV.I32 Q15, #0      (立即数移动: Q15 清零)
    };

    unsigned int len = sizeof(test_simd_insns)/sizeof(test_simd_insns[0]);

    if (init_simd_state_slot() != 0) {
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

    uint8_t insn_bytes[4];
    
    printf("Executing SIMD test...\n");
    
    for(unsigned int i = 0; i < len; i++) {
        size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), test_simd_insns[i]);
        execute_insn_page_reg(insn_bytes, buf_length, NULL);
        
        printf("Instruction: 0x%x\n", test_simd_insns[i]);
        
        // 检查是否有异常发生
        extern volatile sig_atomic_t last_insn_signum;
        if (last_insn_signum != 0) {
            printf("Signal caught: %d (%s)\n", last_insn_signum, strsignal(last_insn_signum));
        } else {
            // 只有在没异常时，对比结果才有意义
            print_simd_diff(&simd_state_base_slot[0], &simd_state_base_slot[1]);
        }
        printf("\n");
    }

    free(simd_state_base_slot);
    return 0;
}
