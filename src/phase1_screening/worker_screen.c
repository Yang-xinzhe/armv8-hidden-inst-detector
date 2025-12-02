#include "core.h"
#include "sandbox.h"

void execution_boilerplate(void);
int init_insn_page(void);
void execute_insn_page(uint8_t*, size_t);
size_t fill_insn_buffer(uint8_t*, size_t, uint32_t);
uint64_t get_nano_timestamp(void);

extern char boilerplate_start, boilerplate_end, insn_location;

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

void execute_insn_page(uint8_t *insn_bytes, size_t insn_length)
{
    // Jumps to the instruction buffer
    void (*exec_page)() = (void (*)())insn_page;

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
    __clear_cache(insn_page + (insn_offset - 1) * 4,
                  insn_page + insn_offset * 4 + insn_length);

    executing_insn = 1;

    // Jump to the instruction to be tested (and execute it)
    if(sigsetjmp(escape_env, 1) == 0) {
        arm_watchdog_us(200);

        exec_page(); 

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


int main(int argc, const char* argv[]) {

}
