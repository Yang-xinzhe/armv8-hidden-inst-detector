#include "core.h"
#include "sandbox.h"
#include "test_runner.h"

#include <unistd.h>
#include <signal.h>
#include <ucontext.h>

// ------------------------------------------------------------------
// Data Structures
// ------------------------------------------------------------------

extern volatile sig_atomic_t last_insn_signum;
extern volatile sig_atomic_t timeout_occurred;
extern sigjmp_buf escape_env;

// 全局变量，记录 Fault PC
volatile uintptr_t fault_pc = 0;
volatile uintptr_t fault_addr = 0;

// [FIX] 使用 sandbox.c 中计算好的全局变量
extern uint32_t insn_offset;

// ------------------------------------------------------------------
// Custom Signal Handler
// ------------------------------------------------------------------

void cf_signal_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc = (ucontext_t *)context;
    
    // 获取报错时的 PC (AArch32)
    // 注意：arm_pc 宏依赖于系统头文件定义，如果跨平台编译可能需要 #ifdef
    fault_pc = uc->uc_mcontext.arm_pc;
    
    // 如果是 SEGV/BUS，记录访问地址
    if (sig == SIGSEGV || sig == SIGBUS) {
        fault_addr = (uintptr_t)info->si_addr;
    }

    if (sig == SIGRTMIN) {
        timeout_occurred = 1;
    }

    last_insn_signum = sig;
    
    // 跳回 execute_insn_page
    siglongjmp(escape_env, 1);
}

// ------------------------------------------------------------------
// TestOps Implementation
// ------------------------------------------------------------------

static int cf_global_init(void) {
    // 覆盖 TestRunner 注册的默认信号处理
    init_signal_handler(cf_signal_handler, SIGILL,    SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGSEGV,   SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGTRAP,   SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGBUS,    SA_SIGINFO);
    init_signal_handler(cf_signal_handler, SIGRTMIN,  SA_SIGINFO | SA_NODEFER);
    init_signal_handler(cf_signal_handler, SIGVTALRM, SA_SIGINFO | SA_NODEFER);
    return 0;
}

static void* cf_create_bitmap(uint32_t start, uint32_t end, const char *out_dir, int file_number) {
    (void)start; (void)end; (void)out_dir; (void)file_number;
    // Control Flow 测试目前只打印 log，不需要记录 bitmap？
    // 原代码没有 create bitmap 的逻辑。
    // 但是 Runner 要求返回非 NULL。
    // 我们返回一个 dummy 指针。
    static int dummy;
    return &dummy;
}

static void cf_run_insn(uint32_t insn, void *bitmap_ptr) {
    (void)bitmap_ptr;
    
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
    
    printf("[*] Testing instruction: 0x%08x ... ", insn);
    fflush(stdout);

    // 重置状态
    fault_pc = 0;
    fault_addr = 0;
    
    // 执行
    execute_insn_page_screen(insn_bytes, buf_length);
    
    // 结果分析
    uintptr_t insn_offset_bytes = insn_offset * 4;
    uintptr_t trap_offset_bytes = insn_offset_bytes + 4;

    if (last_insn_signum == SIGRTMIN || last_insn_signum == SIGALRM) {
        printf("\033[33m[TIMEOUT] Loop/Deadlock detected.\033[0m\n");
    } 
    else if (last_insn_signum == SIGILL || last_insn_signum == SIGTRAP) {
        uintptr_t page_base = (uintptr_t)insn_page;
        uintptr_t pc_offset = fault_pc - page_base;
        
        printf("(Debug: PC Offset: 0x%x, Insn Offset: 0x%x, Trap Offset: 0x%x) ", 
               (uint32_t)pc_offset, (uint32_t)insn_offset_bytes, (uint32_t)trap_offset_bytes);

        if (pc_offset == trap_offset_bytes) {
            printf("\033[32m[NORMAL] Hit Trap (Next Instruction). PC increased sequentially.\033[0m\n");
        } else if (pc_offset == insn_offset_bytes) {
            printf("\033[31m[UNDEF] Instruction itself is Undefined.\033[0m\n");
        } else {
            printf("\033[35m[SIGILL/TRAP] Unknown location: +0x%x\033[0m\n", (uint32_t)pc_offset);
        }
    }
    else if (last_insn_signum == SIGSEGV) {
        printf("\033[36m[JUMP] SegFault at 0x%x (Likely jumped out of sandbox)\033[0m\n", (uint32_t)fault_addr);
    }
    else if (last_insn_signum == 0) {
        printf("[RETURN] Function returned normally (skipped Trap?).\n");
    }
    else {
        printf("[OTHER] Signal: %d\n", last_insn_signum);
    }
}

static int cf_flush_bitmap(void *bitmap_ptr, FILE *file) {
    // Control Flow 不需要写入结果文件，或者我们可以把 log 写入 file？
    // 目前原逻辑只是 print。为了兼容 runner，我们什么都不做。
    return 0;
}

static void cf_destroy_bitmap(void *bitmap_ptr) {
    // Nothing to free
}

// ------------------------------------------------------------------
// Main Entry
// ------------------------------------------------------------------

static const TestOps cf_ops = {
    .test_name = "control_flow_analysis", // Output dir name
    .global_init = cf_global_init,
    .create_bitmap = cf_create_bitmap,
    .run_insn = cf_run_insn,
    .flush_bitmap = cf_flush_bitmap,
    .destroy_bitmap = cf_destroy_bitmap
};

int main(int argc, char *argv[]) {
    return run_test_framework(argc, argv, &cf_ops);
}
