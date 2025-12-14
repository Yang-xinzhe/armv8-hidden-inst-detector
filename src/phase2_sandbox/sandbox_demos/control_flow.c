#include "sandbox.h"

volatile uintptr_t fault_pc = 0;
volatile uintptr_t fault_addr = 0;

void cf_signal_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc = (ucontext_t *)context;
    fault_pc = uc->uc_mcontext.arm_pc;
    
    // 如果是 SEGV/BUS，记录访问地址
    if (sig == SIGSEGV || sig == SIGBUS) {
        fault_addr = (uintptr_t)info->si_addr;
    }

    if (sig == SIGRTMIN) {
        timeout_occurred = 1;
    }

    last_insn_signum = sig;
    
    siglongjmp(escape_env, 1);
}

int main(void)
{
    // 测试指令集：包含 Timeout, Normal(Trap), Jump Out 
    uint32_t test_insns[] = {
        0xEAFFFFFE, // B . (Self Loop)          -> Expect: TIMEOUT (SIGALRM/SIGRTMIN)
        0xEBFFFFFE, // BL . (Call Self)         -> Expect: TIMEOUT
        0xE1A00000, // MOV R0, R0 (NOP)         -> Expect: TRAP (SIGILL at insn+4)
        0xEA000000, // B +4 (Jump to next)      -> Expect: TRAP (SIGILL at insn+4) - Wait, no. B +4 means skip 4 bytes? 
                    // B encoding: offset is 24-bit signed word offset. 
                    // 0xEA000000 -> offset = 0 -> PC += 8 + 0*4 = PC+8.
                    // Current PC is at insn. Trap is at insn+4.
                    // So B +0 (0xEA000000) jumps to insn+8 (after trap).
                    // This should likely CRASH (SIGSEGV) or return normal if code after trap is valid.
        
        0xEafffffC, // B .-4 (Jump to prev)     -> Expect: TIMEOUT or Loop
        0xEA001000, // B +0x4000 (Jump far away) -> Expect: SEGV (Jump Out)
    };
    int len = sizeof(test_insns)/sizeof(test_insns[0]);

    init_signal_handler(cf_signal_handler, SIGILL,    SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGSEGV,   SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGTRAP,   SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGBUS,    SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGRTMIN,  SA_SIGINFO | SA_NODEFER);
    init_signal_handler(cf_signal_handler, SIGVTALRM, SA_SIGINFO | SA_NODEFER);

    if (init_watchdog_timer() != 0) {
        fprintf(stderr, "Failed to initialize watchdog timer\n");
        return 1;
    }

    if (init_insn_page() != 0) {
        perror("init_insn_page");
        return 1;
    }

    printf("Starting Control Flow Analysis Test...\n\n");

    uintptr_t insn_offset_bytes = (uintptr_t)insn_location - (uintptr_t)boilerplate_start;
    uintptr_t trap_offset_bytes = insn_offset_bytes + 4;

    uint8_t insn_bytes[4];
    
    for(int i = 0 ; i < len ; i++) {
        uint32_t insn = test_insns[i];
        size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
        
        printf("[*] Testing instruction: 0x%08x ... ", insn);
        fflush(stdout);

        fault_pc = 0;
        fault_addr = 0;
        
        execute_insn_page_screen(insn_bytes, buf_length);
        
        if (last_insn_signum == SIGRTMIN || last_insn_signum == SIGALRM) {
            printf("\033[33m[TIMEOUT] Loop/Deadlock detected.\033[0m\n");
        } 
        else if (last_insn_signum == SIGILL || last_insn_signum == SIGTRAP) {
            uintptr_t page_base = (uintptr_t)insn_page;
            uintptr_t pc_offset = fault_pc - page_base;
            
            if (pc_offset == trap_offset_bytes) {
                printf("\033[32m[NORMAL] Hit Trap (Next Instruction). PC increased sequentially.\033[0m\n");
            } else if (pc_offset == insn_offset_bytes) {
                printf("\033[31m[UNDEF] Instruction itself is Undefined.\033[0m\n");
            } else {
                printf("\033[35m[SIGILL/TRAP] Unknown location: +0x%x\033[0m\n", pc_offset);
            }
        }
        else if (last_insn_signum == SIGSEGV) {
            printf("\033[36m[JUMP] SegFault at 0x%x (Likely jumped out of sandbox)\033[0m\n", fault_addr);
        }
        else if (last_insn_signum == 0) {
            printf("[RETURN] Function returned normally (skipped Trap?).\n");
        }
        else {
             printf("[OTHER] Signal: %d\n", last_insn_signum);
        }
    }

    return 0;
}
