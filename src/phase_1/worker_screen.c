#include "core.h"

#define PAGE_SIZE 4096

void *insn_page;
volatile sig_atomic_t last_insn_signum = 0;
volatile sig_atomic_t executing_insn = 0;
uint32_t insn_offset = 0;
uint32_t mask = 0x1111;

static uint8_t sig_stack_array[SIGSTKSZ];
stack_t sig_stack = {
    .ss_size = SIGSTKSZ,
    .ss_sp = sig_stack_array,
};

void signal_handler(int, siginfo_t*, void*);
void init_signal_handler(void (*handler)(int, siginfo_t*, void*), int);
void execution_boilerplate(void);
int init_insn_page(void);
void execute_insn_page(uint8_t*, size_t);
size_t fill_insn_buffer(uint8_t*, size_t, uint32_t);
uint64_t get_nano_timestamp(void);

extern char boilerplate_start, boilerplate_end, insn_location;


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
    uintptr_t insn_skip = (uintptr_t)(insn_page) + (insn_offset+1)*4;

    //aarch32
    uc->uc_mcontext.arm_pc = insn_skip;

}

void init_signal_handler(void (*handler)(int, siginfo_t*, void*), int signum)
{
    sigaltstack(&sig_stack, NULL);

    struct sigaction s = {
        .sa_sigaction = handler,
        .sa_flags = SA_SIGINFO | SA_ONSTACK,
    };

    sigfillset(&s.sa_mask);

    sigaction(signum,  &s, NULL);
}


void execution_boilerplate(void)
{
        asm volatile(
            ".global boilerplate_start  \n"
            "boilerplate_start:         \n"

            // Store all gregs
            "push {r0-r12, lr}          \n"

            /*
             * It's better to use ptrace in cases where the sp might
             * be corrupted, but storing the sp in a vector reg
             * mitigates the issue somewhat.
             */
            "vmov s0, sp                \n"

            // Reset the regs to make insn execution deterministic
            // and avoid program corruption
            "mov r0, %[reg_init]        \n"
            "mov r1, %[reg_init]        \n"
            "mov r2, %[reg_init]        \n"
            "mov r3, %[reg_init]        \n"
            "mov r4, %[reg_init]        \n"
            "mov r5, %[reg_init]        \n"
            "mov r6, %[reg_init]        \n"
            "mov r7, %[reg_init]        \n"
            "mov r8, %[reg_init]        \n"
            "mov r9, %[reg_init]        \n"
            "mov r10, %[reg_init]       \n"
            "mov r11, %[reg_init]       \n"
            "mov r12, %[reg_init]       \n"
            "mov lr, %[reg_init]        \n"
            "mov sp, %[reg_init]        \n"

            // Note: this msr insn must be directly above the nop
            // because of the -c option (excluding the label ofc)
           "msr cpsr_f, #0             \n"

            ".global insn_location      \n"
            "insn_location:             \n"

            // This instruction will be replaced with the one to be tested
            "nop                        \n"

            "vmov sp, s0                \n"

            // Restore all gregs
            "pop {r0-r12, lr}           \n"

            "bx lr                      \n"
            ".global boilerplate_end    \n"
            "boilerplate_end:           \n"
            :
            : [reg_init] "n" (0)
            );

}

int init_insn_page(void)
{
    // Allocate an executable page / memory region
    insn_page = mmap(NULL,
                       PAGE_SIZE,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);

    if (insn_page == MAP_FAILED)
        return 1;

    uint32_t boilerplate_length = (&boilerplate_end - &boilerplate_start) / 4;

    // Load the boilerplate assembly
    uint32_t i;
    for ( i = 0; i < boilerplate_length; ++i)
        ((uint32_t*)insn_page)[i] = ((uint32_t*)&boilerplate_start)[i];

    insn_offset = (&insn_location - &boilerplate_start) / 4;

    return 0;
}

void execute_insn_page(uint8_t *insn_bytes, size_t insn_length)
{
    // Jumps to the instruction buffer
    void (*exec_page)() = (void(*)()) insn_page;

    

    // Update the first instruction in the instruction buffer
    memcpy(insn_page + insn_offset * 4, insn_bytes, insn_length);

    last_insn_signum = 0;

    /*
     * Clear insn_page (at the insn to be tested + the msr insn before)
     * in the d- and icache
     * (some instructions might be skipped otherwise.)
     */
    __clear_cache(insn_page + (insn_offset-1) * 4,
                  insn_page + insn_offset * 4 + insn_length);

    executing_insn = 1;

    // Jump to the instruction to be tested (and execute it)
    exec_page();

    executing_insn = 0;

    
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

uint64_t get_nano_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000L + ts.tv_nsec;
}

int main(){
        uint32_t hidden_insn;
        uint32_t cnt=0;
        //uint32_t sigsegv_cnt=0;
        uint32_t sigill_cnt=0;
        //uint32_t sigtrap_cnt=0;
        //uint32_t sigbus_cnt=0;
        uint32_t no_sigsegv=0;
        uint32_t instructions_checked=0;//total udf insns
        time_t start_time = time(NULL);
        init_signal_handler(signal_handler, SIGILL);
       // init_signal_handler(signal_handler, SIGSEGV);
       // init_signal_handler(signal_handler, SIGTRAP);
       // init_signal_handler(signal_handler, SIGBUS);


        if (init_insn_page() != 0) {
            perror("insn_page mmap failed");
            return 1;
        }


        FILE *fp = fopen("/mnt/sd/ua32.txt", "r");
 
        FILE *fp2 = fopen("/mnt/sd/h3_a32.txt", "ab+");
        
        uint64_t last_timestamp = get_nano_timestamp();
        
    
        if (fp == NULL) {
            printf("文件不存在\n");
            return 1;
        }
    
        uint32_t start, end;
        char line[100]; //用于存储每行数据

        while (fgets(line, sizeof(line), fp) != NULL) {
            if (sscanf(line, "[%d, %d]", &start, &end) == 2) {
               for (uint32_t i = start; i < end; i++) {
                    //printf("%d ", i);
                    hidden_insn = i;

                    cnt++;
                    int flag=0;
                
                    
                    //if (cnt%0x10000==0){
                    //    flag=1;
                    //}

                    
                    uint8_t insn_bytes[4];
                    size_t buf_length = fill_insn_buffer(insn_bytes,
                                                        sizeof(insn_bytes),
                                                        hidden_insn);

                    execute_insn_page(insn_bytes, buf_length);

                    if (last_insn_signum == SIGILL) {
                        sigill_cnt++;
                        //if (flag==1){
                            printf("0x%08x-- no_sigsegv=%d  sigill_cnt=%d\n",hidden_insn,no_sigsegv,sigill_cnt);
                       //}
                    }
                    else{
                            no_sigsegv++;
                           // if (flag==1){
                                printf("0x%08x-- no_sigsegv=%d  sigill_cnt=%d\n",hidden_insn,no_sigsegv,sigill_cnt);
                           // }
                            fwrite(&hidden_insn,4,1,fp2);
                    }
                    





                    instructions_checked++;
                } 
                //printf("\n");
            }
        }
        munmap(insn_page, PAGE_SIZE);
        fclose(fp);
        fclose(fp2);
        printf("Total insn numbers (checked):%d \n", instructions_checked);
    
    
            printf("HIDDEN insn numbers:%d\n",no_sigsegv);
        
        return 0;
        
}

