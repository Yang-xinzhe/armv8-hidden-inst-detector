#include "core.h"
#include "sandbox.h"
#include "bitmap.h"

void execution_boilerplate(void);

void execution_boilerplate(void)
{
        __asm__ __volatile__(
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


static int count_ranges_in_file(FILE *f, uint64_t *total_insns_out)
{
    char line[256];
    int count = 0;
    uint32_t start, end;
    uint64_t total = 0;

    fseek(f, 0, SEEK_SET);

    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "[%u, %u]", &start, &end) == 2) {
            count++;
            if (end > start) {
                total += (uint64_t)(end - start);
            }
        }
    }

    fseek(f, 0, SEEK_SET);
    if (total_insns_out) {
        *total_insns_out = total;
    }
    return count;
}

int main(int argc, const char* argv[]) {
    
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <file_number>\n", argv[0]);
        fprintf(stderr, "Example: %s 1  # Handling results_A32/res1.txt\n", argv[0]);
        return 1;
    }

    int target_file_num = atoi(argv[1]);
    int file_number = target_file_num;
    
    char file_num_env[32];
    snprintf(file_num_env, sizeof(file_num_env), "%d", file_number);
    setenv("RESULT_FILE_NUMBER", file_num_env, 1);

    // Ensure signal unmasked
    sigset_t empty_set;
    sigemptyset(&empty_set);
    pthread_sigmask(SIG_SETMASK, &empty_set, NULL);


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
        perror("insn_page mmap failed");
        timer_delete(watchdog_timer);
        return 1;
    }

    char input_filename[256];
    snprintf(input_filename, sizeof(input_filename), "results_A32/res%d.txt", target_file_num);

    FILE *res_file = fopen(input_filename, "r");
    if (!res_file) {
        perror("fopen res_file");
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    uint64_t total_insns = 0;
    int range_count = count_ranges_in_file(res_file, &total_insns);
    if (range_count == 0) {
        printf("[res%d] invalid \n", file_number);
        fclose(res_file);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 0;
    }

    mkdir("bitmap_results", 0755);

    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename),
             "bitmap_results/res%d_complete.bin", file_number);

    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        fprintf(stderr, "failed to create %s\n", output_filename);
        fclose(res_file);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    char timeout_filename[256];
    snprintf(timeout_filename, sizeof(timeout_filename),
             "bitmap_results/res%d_timeout.bin", file_number);

    FILE *timeout_file = fopen(timeout_filename, "wb");
    if (!timeout_file) {
        fprintf(stderr, "failed to create %s\n", timeout_filename);
        fclose(output_file);
        fclose(res_file);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    // complete header：[file_number][range_count]
    fwrite(&file_number, sizeof(int), 1, output_file);
    fwrite(&range_count, sizeof(int), 1, output_file);

    // timeout header：[file_number][timeout_range_count]，will be write back
    int timeout_range_count = 0;
    fwrite(&file_number, sizeof(int), 1, timeout_file);
    fwrite(&timeout_range_count, sizeof(int), 1, timeout_file); // write 0 first

    char line[256];
    int  current_range_index = 0;

    while (fgets(line, sizeof(line), res_file) != NULL) {
        uint32_t range_start, range_end;
        if (sscanf(line, "[%u, %u]", &range_start, &range_end) != 2) {
            continue;
        }

        if (range_end <= range_start) {
            continue;
        }

        current_range_index++;

        RangeBitmap rb;
        if (range_bitmap_init(&rb, range_start, range_end) != 0) {
            fprintf(stderr, "\n[res%d] range_bitmap_init failed for [%u, %u)\n",
                    file_number, range_start, range_end);
            continue;
        }

        for (uint32_t insn = range_start; insn < range_end; ++insn) {

            uint8_t insn_bytes[4];
            size_t buf_len = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);

            execute_insn_page(insn_bytes, buf_len, NULL);

            if (last_insn_signum == SIGALRM || last_insn_signum == SIGPROF) {
                range_bitmap_mark_timeout(&rb, insn);
            } else if (last_insn_signum == 0) {
                range_bitmap_mark_exec(&rb, insn);
            } else {
                // crash
            }
        }

        int flush_ret = range_bitmap_flush(&rb, output_file, timeout_file);
        if (flush_ret < 0) {
            fprintf(stderr, "\n[res%d] range_bitmap_flush failed for [%u, %u)\n",
                    file_number, range_start, range_end);
            range_bitmap_destroy(&rb);
            break;
        }
        if (flush_ret == 1) {
            timeout_range_count++;
        }

        range_bitmap_destroy(&rb);
    }

    fseek(timeout_file, sizeof(int), SEEK_SET);
    fwrite(&timeout_range_count, sizeof(int), 1, timeout_file);

    fclose(output_file);
    fclose(timeout_file);
    fclose(res_file);

    timer_delete(watchdog_timer);
    munmap(insn_region, PAGE_SIZE * 3);
    return 0;
}
