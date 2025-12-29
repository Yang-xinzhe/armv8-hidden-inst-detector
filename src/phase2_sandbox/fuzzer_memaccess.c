#include "core.h"
#include "bitmap.h"
#include "sandbox.h"
#include "pmu_counter.h"
#include "config.h"
#include "fs_utils.h"

#include <string.h>
#include <unistd.h>

extern volatile sig_atomic_t last_insn_signum;

static const char *DEFAULT_CONFIG_PATH = "config/project.conf";

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-f <config_path>] [-i <input_dir>] [-o <output_base>] <file_number>\n", prog);
    fprintf(stderr, "  -f: config file path (default: %s)\n", DEFAULT_CONFIG_PATH);
    fprintf(stderr, "  -i: override phase2_input_dir\n");
    fprintf(stderr, "  -o: override phase2_output_dir (base directory)\n");
}

static void build_subdir(char *out, size_t out_sz, const char *base, const char *subdir)
{
    if (!base || base[0] == '\0') {
        snprintf(out, out_sz, "%s", subdir);
        return;
    }
    size_t n = strlen(base);
    if (n > 0 && base[n - 1] == '/') {
        snprintf(out, out_sz, "%s%s", base, subdir);
    } else {
        snprintf(out, out_sz, "%s/%s", base, subdir);
    }
}

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

int main(int argc, char *argv[]) {

    ProjectConfig cfg;
    project_config_init(&cfg);
    const char *config_path = DEFAULT_CONFIG_PATH;
    const char *cli_input_dir = NULL;
    const char *cli_output_base = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "f:i:o:")) != -1) {
        switch (opt) {
            case 'f': config_path = optarg; break;
            case 'i': cli_input_dir = optarg; break;
            case 'o': cli_output_base = optarg; break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    int target_file_num = atoi(argv[optind]);
    int file_number = target_file_num;

    (void)project_config_load(&cfg, config_path);

    const char *input_dir = cfg.phase2_input_dir;
    const char *output_base = cfg.phase2_output_dir;
    if (cli_input_dir && cli_input_dir[0] != '\0') input_dir = cli_input_dir;
    if (cli_output_base) output_base = cli_output_base;


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
        perror("init_insn_page");
        return 1;
    }

    char input_filename[512];
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

    char out_dir[512];
    build_subdir(out_dir, sizeof(out_dir), output_base, "memaccess_results");
    if (mkdir_p(out_dir, 0755) != 0) {
        perror("mkdir_p memaccess_results");
        return 1;
    }

    char output_filename[768];
    snprintf(output_filename, sizeof(output_filename),
             "%s/res%d_complete.bin", out_dir, file_number);

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
            
            last_insn_signum = 0;
            execute_insn_page_pmu(insn_bytes, buf_length, &res);

            if (last_insn_signum != 0) {
                // printf("Signal %d caught for %x, ignoring PMU\n", last_insn_signum, insn);
                continue; 
            }

            if (res.ld_result > 0 && res.st_result > 0) {
                continue;
            }

            if (res.ld_result > 0 && res.ld_result < 20) {
                range_bitmap_mark_ld(&rb, insn);
                // printf("ldr: 0x%x\n", insn);
            } 
            if (res.st_result > 0 && res.st_result < 20) {
                range_bitmap_mark_st(&rb, insn);
                // printf("str: 0x%x\n", insn);
            }
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
