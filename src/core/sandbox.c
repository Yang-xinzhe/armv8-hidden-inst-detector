#include "sandbox.h"


void *insn_region = NULL;                  // [guard][code][guard]
void *insn_page   = NULL;                  // Executable Page

volatile sig_atomic_t last_insn_signum  = 0;
volatile sig_atomic_t executing_insn    = 0;
volatile sig_atomic_t timeout_occurred  = 0;

uint32_t insn_offset = 0;
uint32_t mask        = 0x1111;

sigjmp_buf escape_env;

timer_t watchdog_timer;

uint8_t sig_stack_array[MY_SIGSTKSZ];

stack_t sig_stack = {
    .ss_size = MY_SIGSTKSZ,
    .ss_sp   = sig_stack_array,
};

void signal_handler(int sig_num, siginfo_t *sig_info, void *uc_ptr)
{
    // Suppress unused warning
    (void)sig_info;

    ucontext_t* uc = (ucontext_t*) uc_ptr;

    last_insn_signum = sig_num;


    if (executing_insn == 0) {
        // Something other than a hidden insn execution raised the signal,
        // so quit
        fprintf(stderr, "%s\n", strsignal(sig_num));
        exit(1);
    }

    // Jump to the next instruction (i.e. skip the illegal insn)
    // uintptr_t insn_skip = (uintptr_t)(insn_page) + (insn_offset+1)*4;

    // //aarch32
    // uc->uc_mcontext.arm_pc = insn_skip;

    (void)uc; 
    siglongjmp(escape_env, sig_num);
}

void init_signal_handler(void (*handler)(int, siginfo_t*, void*), int signum, int flags)
{
    sigaltstack(&sig_stack, NULL);

    struct sigaction s = {
        .sa_sigaction = handler,
        // HINT: Timeout handler using SA_NODEFER
        // TODO: Add SA_RESTART back?
        .sa_flags = SA_SIGINFO | SA_ONSTACK | flags,
    };

    sigemptyset(&s.sa_mask);

    sigaction(signum,  &s, NULL);
}

int init_insn_page(void)
{
    // Allocate an executable page / memory region
    insn_region = mmap(NULL,
                       PAGE_SIZE * 3,
                       PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);

    if (insn_region == MAP_FAILED)
        return 1;

    insn_page = (uint8_t*)insn_region + PAGE_SIZE;

    if (mprotect(insn_page, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        munmap(insn_region, PAGE_SIZE * 3);
        return 1;
    }

    uint32_t boilerplate_length = (&boilerplate_end - &boilerplate_start) / 4;

    // Load the boilerplate assembly
    uint32_t i;
    for (i = 0; i < boilerplate_length; ++i)
        ((uint32_t *)insn_page)[i] = ((uint32_t *)&boilerplate_start)[i];

    insn_offset = (&insn_location - &boilerplate_start) / 4;

    if (mprotect(insn_page, PAGE_SIZE, PROT_READ | PROT_EXEC) != 0) {
        munmap(insn_region, PAGE_SIZE * 3);
        return 1;
    }

    return 0;
}

void execute_insn_page(uint8_t *insn_bytes, size_t insn_length, void *ctx, 
                       converge_exec_t pre_exec, 
                       exec_t exec_cb, 
                       converge_exec_t post_exec)
{
    if (mprotect(insn_page, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("mprotect RWX failed");
        return;
    }

    // Update the first instruction in the instruction buffer
    memcpy(insn_page + insn_offset * 4, insn_bytes, insn_length);

    last_insn_signum = 0;
    timeout_occurred = 0;

    /*
     * Clear insn_page (at the insn to be tested + the msr insn before)
     * in the d- and icache
     * (some instructions might be skipped otherwise.)
     */
    __builtin___clear_cache(insn_page + (insn_offset - 1) * 4,
                  insn_page + insn_offset * 4 + insn_length);

    executing_insn = 1;

    // Jump to the instruction to be tested (and execute it)
    if(sigsetjmp(escape_env, 1) == 0) {
        arm_watchdog_us(200);

        if(pre_exec) pre_exec(ctx);

        exec_cb(insn_page, ctx);

        if(post_exec) post_exec(ctx);

        disarm_watchdog();
    } else {
        disarm_watchdog();

        if (timeout_occurred) {
            last_insn_signum = SIGALRM;
        }
    }
    
    executing_insn = 0;

    if (mprotect(insn_page, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("mprotect restore RWX failed");
    }

}

static void exec_std(void *addr, void *ctx) {
    (void)ctx;
    void (*exec_page)() = (void (*)())addr;
    exec_page();
}

static void exec_reg(void *addr, void *ctx) {
    RegisterStates *states = (RegisterStates *)ctx;
    void (*exec_page)(RegisterStates*) = (void (*)(RegisterStates*))addr;
    exec_page(states);
}

void execute_insn_page_screen(uint8_t *insn_bytes, size_t insn_length) {
    execute_insn_page(insn_bytes, insn_length, NULL, NULL, exec_std, NULL);
}

void execute_insn_page_reg(uint8_t *insn_bytes, size_t insn_length, RegisterStates *states) {
    execute_insn_page(insn_bytes, insn_length, states, NULL, exec_reg, NULL);
}


size_t fill_insn_buffer(uint8_t *buf, size_t buf_size, uint32_t insn)
{
    if (buf_size < 4)
        return 0;
 
    else {
        buf[0] = insn & 0xff;
        buf[1] = (insn >> 8) & 0xff;
        buf[2] = (insn >> 16) & 0xff;
        buf[3] = (insn >> 24) & 0xff;
    }
    return 4;
}

static pid_t gettid_wrapper(void) {
    return syscall(SYS_gettid);
}

int init_watchdog_timer(void) {
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGRTMIN; 
    sev._sigev_un._tid = gettid_wrapper(); 
    
    if (timer_create(CLOCK_MONOTONIC, &sev, &watchdog_timer) != 0) {
        perror("timer_create failed");
        return -1;
    }
    return 0;
}

void arm_watchdog_us(int us) {
    struct itimerspec its = {
        .it_value.tv_sec = 0,
        .it_value.tv_nsec = us * 1000,
        .it_interval = {0, 0}
    };
    timer_settime(watchdog_timer, 0, &its, NULL);
}


void disarm_watchdog(void) {
    struct itimerspec its = {{0, 0}, {0, 0}};
    timer_settime(watchdog_timer, 0, &its, NULL);
}