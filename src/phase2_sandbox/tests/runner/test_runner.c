#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

#include "test_runner.h"
#include "config.h"
#include "fs_utils.h"
#include "sandbox.h"
#include "cpu_affinity.h"

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

int run_test_framework(int argc, char *argv[], const TestOps *ops)
{
    if (!ops || !ops->run_insn || !ops->create_bitmap || !ops->flush_bitmap || !ops->destroy_bitmap) {
        fprintf(stderr, "Error: Invalid TestOps\n");
        return 1;
    }

    // 1. Argument Parsing
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
    
    // Load Config
    (void)project_config_load(&cfg, config_path);
    const char *input_dir = cfg.phase2_input_dir;
    const char *output_base = cfg.phase2_output_dir;
    if (cli_input_dir && cli_input_dir[0] != '\0') input_dir = cli_input_dir;
    if (cli_output_base) output_base = cli_output_base;

    // 2. System Setup (Signal, Watchdog, Sandbox)
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

    // 3. Global Initialization (Optional)
    // Runs AFTER default signal setup, allowing override.
    if (ops->global_init) {
        if (ops->global_init() != 0) {
            fprintf(stderr, "Global initialization failed\n");
            return 1;
        }
    }

    // 4. Open Input File (Binary Mode)
    char input_filename[512];
    // Check for candidates_X.bin first (new format), fallback to resX.bin/txt if needed?
    // Let's stick to the new standard: candidates_{N}_complete.bin
    // WAIT: The python script outputs candidates_{N}_complete.bin
    // But Phase 2 usually tests ALL candidates (both complete and timeout from Phase 1)?
    // For simplicity, let's assume the user passes the specific file number, and we look for candidates_X_complete.bin
    // OR we iterate both? 
    // Usually Phase 2 iterates what was found in Phase 1.
    // Let's try to open "candidates_{N}_complete.bin". If not found, maybe try "candidates_{N}_timeout.bin"?
    // Or maybe we should just accept the filename pattern or handle "res%d.bin" if we renamed it?
    // 
    // In our previous discussion, we decided phase1_decode.py outputs `candidates_{file_num}_complete.bin`.
    // Let's assume input_dir contains these files.
    
    // Attempt 1: candidates_{N}_complete.bin
    snprintf(input_filename, sizeof(input_filename), "%s/candidates_%d_complete.bin", input_dir, target_file_num);
    FILE *res_file = fopen(input_filename, "rb");
    if (!res_file) {
        // Attempt 2: Maybe it was a timeout file?
        snprintf(input_filename, sizeof(input_filename), "%s/candidates_%d_timeout.bin", input_dir, target_file_num);
        res_file = fopen(input_filename, "rb");
    }
    
    if (!res_file) {
        // Last resort: maybe legacy name? Or user provided full path in input_dir?
        // Let's just fail and print error.
        fprintf(stderr, "Cannot open input file for %d in %s (tried candidates_%%d_complete/timeout.bin)\n", 
                target_file_num, input_dir);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 1;
    }

    // 5. Read Binary Header
    RangeFileHeader header;
    if (fread(&header, sizeof(RangeFileHeader), 1, res_file) != 1) {
        fprintf(stderr, "Failed to read file header\n");
        fclose(res_file);
        return 1;
    }

    if (header.magic != HIDR_MAGIC) {
        fprintf(stderr, "Invalid file magic: 0x%x (expected 0x%x)\n", header.magic, HIDR_MAGIC);
        fclose(res_file);
        return 1;
    }

    if (header.count == 0) {
        printf("File %d has 0 ranges, skipping.\n", target_file_num);
        fclose(res_file);
        munmap(insn_region, PAGE_SIZE * 3);
        timer_delete(watchdog_timer);
        return 0;
    }

    // 6. Prepare Output Directory
    char out_dir[512];
    build_subdir(out_dir, sizeof(out_dir), output_base, ops->test_name);
    if (mkdir_p(out_dir, 0755) != 0) {
        perror("mkdir_p output dir");
        return 1;
    }

    char output_filename[768];
    snprintf(output_filename, sizeof(output_filename),
             "%s/res%d_complete.bin", out_dir, target_file_num);

    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        perror("fopen output_file");
        fclose(res_file);
        return 1;
    }

    // Write Output Header: [file_num][range_count]
    fwrite(&target_file_num, sizeof(int), 1, output_file);
    fwrite(&header.count, sizeof(int), 1, output_file);

    // 7. Loop Ranges
    for (uint32_t i = 0; i < header.count; ++i) {
        RangeEntry range;
        if (fread(&range, sizeof(RangeEntry), 1, res_file) != 1) {
            fprintf(stderr, "Unexpected EOF reading range %d\n", i);
            break;
        }

        if (range.end <= range.start) continue;

        void *bitmap = ops->create_bitmap(range.start, range.end, out_dir, target_file_num);
        if (!bitmap) {
            fprintf(stderr, "Failed to create bitmap for range [%x, %x)\n", range.start, range.end);
            continue;
        }

        // Loop Instructions
        for (uint32_t insn = range.start; insn < range.end; ++insn) {
            // 注意：这里我们假设步长为 1 (i.e. insn++)
            // 但如果是在 A32 模式下，是否应该 +4？
            // 目前你的所有代码（bitmap逻辑）都是基于 insn++ 的（index），
            // 只要 fill_insn_buffer 处理正确，这里 +1 没问题。
            // 实际上 insn 是指令的值（或地址？）。如果是值，+1 意味着测试每一个可能的编码。
            // 你的 Phase 1 也是 ++insn，所以这里保持一致。
            
            ops->run_insn(insn, bitmap);
        }

        if (ops->flush_bitmap(bitmap, output_file) != 0) {
            fprintf(stderr, "Failed to flush bitmap for range [%x, %x)\n", range.start, range.end);
        }

        ops->destroy_bitmap(bitmap);
    }

    fclose(output_file);
    fclose(res_file);
    munmap(insn_region, PAGE_SIZE * 3);
    timer_delete(watchdog_timer);

    return 0;
}
