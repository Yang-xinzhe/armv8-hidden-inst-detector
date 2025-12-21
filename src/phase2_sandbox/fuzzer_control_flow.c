#include "sandbox.h"
#include "config.h"

// 全局变量，记录 Fault PC
volatile uintptr_t fault_pc = 0;
volatile uintptr_t fault_addr = 0;

// 自定义信号处理函数，用于捕获 PC
void cf_signal_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc = (ucontext_t *)context;
    // 获取报错时的 PC (AArch32)
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

int main(int argc, const char* argv[])
{
    int target_file_num = atoi(argv[1]);
    int file_number = target_file_num;

    ProjectConfig cfg;
    project_config_init(&cfg);
    (void)project_config_load(&cfg, "config/project.conf");
    const char *input_dir = cfg.phase2_input_dir;

    sigset_t empty_set;
    sigemptyset(&empty_set);
    pthread_sigmask(SIG_SETMASK, &empty_set, NULL);

    // 初始化信号处理
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

    // 注意：init_insn_page 会复制 boilerplate 汇编代码到内存页
    // 这里我们使用的是 linked 的 control_flow_asm.o 里的代码
    if (init_insn_page() != 0) {
        perror("init_insn_page");
        return 1;
    }

    char input_filename[256];
    snprintf(input_filename, sizeof(input_filename), "%s/res%d_timeout_decoded.txt", input_dir, target_file_num);

    FILE *res_file = fopen(input_filename, "r");
    if (!res_file) {
        perror("fopen res_file");
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    printf("Starting Control Flow Analysis Test...\n\n");

    // [FIX] 使用 sandbox.c 中计算好的全局变量，避免链接符号地址计算错误
    extern uint32_t insn_offset;
    uintptr_t insn_offset_bytes = insn_offset * 4;
    
    // Trap 指令在待测指令后 4 字节
    uintptr_t trap_offset_bytes = insn_offset_bytes + 4;

    char line[256];
    int  current_range_index = 0;

    while (fgets(line, sizeof(line), res_file) != NULL) {
        uint32_t range_start, range_end;
        if (sscanf(line, "[%x, %x]", &range_start, &range_end) != 2) {
            continue;
        }

        if (range_end <= range_start) {
            continue;
        }

        for (uint32_t insn = range_start; insn < range_end; ++insn) {
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
            if (last_insn_signum == SIGRTMIN || last_insn_signum == SIGALRM) {
                printf("\033[33m[TIMEOUT] Loop/Deadlock detected.\033[0m\n");
            } 
            else if (last_insn_signum == SIGILL || last_insn_signum == SIGTRAP) {
                // 计算 Fault PC 相对于 Page 的偏移
                uintptr_t page_base = (uintptr_t)insn_page;
                uintptr_t pc_offset = fault_pc - page_base;
                
                // Debug Print
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
                printf("\033[36m[JUMP] SegFault at 0x%x (Likely jumped out of sandbox)\033[0m\n", fault_addr);
            }
            else if (last_insn_signum == 0) {
                // 如果信号是 0，说明正常返回了？
                // 我们的汇编里如果 Trap 没触发，后面是恢复现场返回
                printf("[RETURN] Function returned normally (skipped Trap?).\n");
            }
            else {
                printf("[OTHER] Signal: %d\n", last_insn_signum);
            }
        }
    }

    return 0;
}
