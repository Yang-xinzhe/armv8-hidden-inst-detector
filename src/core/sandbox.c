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