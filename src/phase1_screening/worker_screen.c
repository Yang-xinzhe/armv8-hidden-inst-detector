#include "core.h"
#include "sandbox.h"
#include "bitmap.h"
#include "config.h"

void execution_boilerplate(void);

#if defined(TEST_T32_16BIT)
void execution_boilerplate(void)
{
    __asm__ __volatile__(
        ".syntax unified            \n"
        ".thumb                     \n"
        ".align 2                   \n"
        
        ".global boilerplate_start  \n"
        "boilerplate_start:         \n"

        "push {r0-r12, lr}          \n" 
        "vmov s0, sp                \n"

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

        "msr APSR_nzcvq, r0          \n"

        ".align 2                   \n"
        ".global insn_location      \n"
        "insn_location:             \n"
        // 16bits padding
        ".space 4                   \n" 

        "vmov sp, s0                \n"
        "pop {r0-r12, lr}           \n"
        "bx lr                      \n"
        
        ".align 2                   \n"
        ".global boilerplate_end    \n"
        "boilerplate_end:           \n"
        :
        : [reg_init] "n" (0)
        );
}

#elif defined(TEST_T32_32BIT)
__attribute__((target("thumb"))) 
void execution_boilerplate(void)
{
    __asm__ __volatile__(
        ".syntax unified            \n"
        ".thumb                     \n"
        ".align 2                   \n"
        
        ".global boilerplate_start  \n"
        "boilerplate_start:         \n"

        "push {r0-r12, lr}          \n" 
        "vmov s0, sp                \n"

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

        "msr APSR_nzcvq, r0          \n"

        ".align 2                   \n"
        ".global insn_location      \n"
        "insn_location:             \n"
        ".space 4                   \n" 

        "vmov sp, s0                \n"
        "pop {r0-r12, lr}           \n"
        "bx lr                      \n"

        ".align 2                   \n"
        ".global boilerplate_end    \n"
        "boilerplate_end:           \n"
        :
        : [reg_init] "n" (0)
        );
}

#else
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
#endif

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

int main(int argc, char *argv[]) {
    // Initialize with invalid values to force specification
    char *input_dir = NULL;
    char *output_dir = NULL;
    int target_file_num = -1;

    // Parse arguments: -i input -o output <file_num>
    int opt;
    while ((opt = getopt(argc, argv, "i:o:")) != -1) {
        switch (opt) {
            case 'i': input_dir = optarg; break;
            case 'o': output_dir = optarg; break;
            default:
                fprintf(stderr, "Usage: %s -i <input_dir> -o <output_dir> <file_number>\n", argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        target_file_num = atoi(argv[optind]);
    }

    if (target_file_num < 0 || !input_dir || !output_dir) {
        fprintf(stderr, "Error: Missing required arguments.\n");
        fprintf(stderr, "Usage: %s -i <input_dir> -o <output_dir> <file_number>\n", argv[0]);
        return 1;
    }

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
    init_signal_handler(signal_handler, SIGABRT,   SA_NONE);

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
    snprintf(input_filename, sizeof(input_filename), "%s/res%d.txt", input_dir, target_file_num);

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

    mkdir(output_dir, 0755);

    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename),
             "%s/candidates_%d_complete.bin", output_dir, file_number);

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
             "%s/candidates_%d_timeout.bin", output_dir, file_number);

    FILE *timeout_file = fopen(timeout_filename, "wb");
    if (!timeout_file) {
        fprintf(stderr, "failed to create %s\n", timeout_filename);
        fclose(output_file);
        fclose(res_file);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    // Initialize HIDR Headers
    RangeFileHeader header = {
        .magic = HIDR_MAGIC,
        .version = HIDR_VERSION,
        .count = 0,
        .reserved = 0
    };
    fwrite(&header, sizeof(header), 1, output_file);
    fwrite(&header, sizeof(header), 1, timeout_file);

    int total_complete_ranges = 0;
    int total_timeout_ranges = 0;

    char line[256];
    int  current_range_index = 0;

#if defined(TEST_T32_16BIT) || defined(TEST_T32_32BIT)
    extern int g_sandbox_thumb_mode;
    g_sandbox_thumb_mode = 1;
#endif

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
            
            // 重置 insn_page 模板以防止自修改代码污染
            munmap(insn_region, PAGE_SIZE * 3);
            if (init_insn_page() != 0) {
                perror("init_insn_page failed in loop");
                break;
            }
            uint32_t insn_to_write = insn;
        #if defined(TEST_T32_16BIT)
            insn_to_write = (insn & 0xFFFF) | (0xBF00 << 16); // 0xBF Thumb NOP
        #endif
            uint8_t insn_bytes[4];
            size_t buf_len = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn_to_write);

            execute_insn_page_screen(insn_bytes, buf_len);

            if (last_insn_signum == SIGALRM || last_insn_signum == SIGPROF) {
                range_bitmap_mark_timeout(&rb, insn);
            } else if (last_insn_signum == 0) {
                range_bitmap_mark_exec(&rb, insn);
            } else {
                // crash
            }
        }

        int c_cnt = range_bitmap_write_ranges(&rb, RB_PLANE_EXEC, output_file);
        if (c_cnt < 0) {
            fprintf(stderr, "Error writing complete ranges\n");
        } else {
            total_complete_ranges += c_cnt;
        }

        int t_cnt = range_bitmap_write_ranges(&rb, RB_PLANE_TIMEOUT, timeout_file);
        if (t_cnt < 0) {
            fprintf(stderr, "Error writing timeout ranges\n");
        } else {
            total_timeout_ranges += t_cnt;
        }

        range_bitmap_destroy(&rb);
    }

    // Rewrite headers with actual counts
    header.count = total_complete_ranges;
    fseek(output_file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, output_file);

    header.count = total_timeout_ranges;
    fseek(timeout_file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, timeout_file);

    fclose(output_file);
    fclose(timeout_file);
    fclose(res_file);

    timer_delete(watchdog_timer);
    munmap(insn_region, PAGE_SIZE * 3);
    return 0;
}
