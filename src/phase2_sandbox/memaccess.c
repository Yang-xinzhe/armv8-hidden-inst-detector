#include "core.h"
#include "bitmap.h"
#include "sandbox.h"
#include "pmu_counter.h"

extern volatile sig_atomic_t last_insn_signum;

static int count_ranges_in_file(FILE *f, uint64_t *total_insns_out)
{
    char line[256];
    int count = 0;
    uint32_t start, end;
    uint64_t total = 0;

    fseek(f, 0, SEEK_SET);

    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "[%x, %x]", &start, &end) == 2) {
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

int main(int argc, const char *argv[]) {

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <file_number>\n", argv[0]);
        fprintf(stderr, "Example: %s 1  # Handling hidden_insn/res1.txt\n", argv[0]);
        return 1;
    }

    int target_file_num = atoi(argv[1]);
    int file_number = target_file_num;


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
        perror("init_insn_page");
        return 1;
    }

    char input_filename[256];
    snprintf(input_filename, sizeof(input_filename), "hidden_insn/res%d.txt", target_file_num);

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

    mkdir("memaccess_results", 0755);

    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename),
             "memaccess_results/res%d_complete.bin", file_number);

    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        fprintf(stderr, "failed to create %s\n", output_filename);
        fclose(res_file);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    // complete header：[file_number][range_count]
    fwrite(&file_number, sizeof(int), 1, output_file);
    fwrite(&range_count, sizeof(int), 1, output_file);

    char line[256];
    int  current_range_index = 0;

    // pmu_init();

    while (fgets(line, sizeof(line), res_file) != NULL) {
        uint32_t range_start, range_end;
        if (sscanf(line, "[%x, %x]", &range_start, &range_end) != 2) {
            continue;
        }

        if (range_end <= range_start) {
            continue;
        }

        current_range_index++;

        RangeBitmap rb;
        // 使用 RB_MASK_LD | RB_MASK_ST 仅追踪访存指令
        if (range_bitmap_init_with_mask(&rb, range_start, range_end, RB_MASK_LD | RB_MASK_ST) != 0) {
            fprintf(stderr, "[res%d] range_bitmap_init_with_mask failed for [%u, %u)\n",
                    file_number, range_start, range_end);
            continue;
        }
        
        for (uint32_t insn = range_start; insn < range_end; ++insn) {
            munmap(insn_region, PAGE_SIZE * 3);
            if (init_insn_page() != 0) break;
            pmu_init();
            uint8_t insn_bytes[4];
            size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
            PmuResult res = {0};
            execute_insn_page_pmu(insn_bytes, buf_length, &res);
            if (last_insn_signum != 0) {
                printf("Signal %d caught for %x, ignoring PMU\n", last_insn_signum, insn);
                continue; 
            }
            

            if (res.ld_result > 0 || res.st_result > 0) {
                printf("Ins: %08x | Sig: %d | LD: %u | ST: %u\n", 
                       insn, last_insn_signum, res.ld_result, res.st_result);
             }

            // if (res.ld_result > 0 && res.ld_result < 20) {
            //     range_bitmap_mark_ld(&rb, insn);
            //     // printf("ldr: 0x%x\n", insn);
            // } 
            // if (res.st_result > 0 && res.st_result < 20) {
            //     range_bitmap_mark_st(&rb, insn);
            //     // printf("str: 0x%x\n", insn);
            // }
            
        }

        if (range_bitmap_serialize(&rb, output_file) != 0) {
            fprintf(stderr, "Failed to flush bitmap for range [0x%x, 0x%x]\n", 
                    range_start, range_end);
        }

        range_bitmap_destroy(&rb);
    }
    // printf("\n");
    return 0;
}
