#include "core.h"
#include "register_states.h"
#include "bitmap.h"
#include "sandbox.h"
#include "cpu_affinity.h"
#include "config.h"
#include "fs_utils.h"

#include <string.h>
#include <unistd.h>

typedef struct {
    uint32_t insn;
    uint32_t before;
    uint32_t after;
} CpsrChangeLog;

RegisterStates *reg_state_base_slot = NULL; 

typedef struct {
    RangeBitmap rb;
} RegEffectBitmap;

static int reg_bitmap_init(RegEffectBitmap *rb, uint32_t start, uint32_t end)
{
    if (!rb) return -1;
    return range_bitmap_init_with_mask(&rb->rb, start, end, RB_MASK_GPR | RB_MASK_CPSR | RB_MASK_SP);
}

static void reg_bitmap_mark_gpr(RegEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_GPR, insn);
}

static void reg_bitmap_mark_cpsr(RegEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_CPSR, insn);
}

static void reg_bitmap_mark_sp(RegEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_SP, insn);
}

static int reg_bitmap_flush(RegEffectBitmap *rb, FILE *file)
{
    if (!rb || !file) return -1;
    const RangeBitmap *r = &rb->rb;
    if (fwrite(&r->start, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->end,   sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->size,  sizeof(uint32_t), 1, file) != 1) return -1;

    /* 写 gpr_changed 位图（RB_PLANE_GPR），即使全 0 也写，方便解析保持定长 */
    if (!(r->plane_mask & RB_MASK_GPR) || !r->planes[RB_PLANE_GPR]) return -1;
    if (fwrite(r->planes[RB_PLANE_GPR], 1, r->size, file) != r->size) return -1;

    /* 写 cpsr_changed 位图（RB_PLANE_CPSR），即使全 0 也写 */
    if (!(r->plane_mask & RB_MASK_CPSR) || !r->planes[RB_PLANE_CPSR]) return -1;
    if (fwrite(r->planes[RB_PLANE_CPSR], 1, r->size, file) != r->size) return -1;

    /* 写 sp_changed 位图 (RB_PLANE_SP)，即使全 0 也写，保持格式一致 */
    if (!(r->plane_mask & RB_MASK_SP) || !r->planes[RB_PLANE_SP]) return -1;
    if (fwrite(r->planes[RB_PLANE_SP], 1, r->size, file) != r->size) return -1;

    return 0;
}

static void reg_bitmap_destroy(RegEffectBitmap *rb)
{
    if (!rb) return;
    range_bitmap_destroy(&rb->rb);
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

static const char *DEFAULT_CONFIG_PATH = "config/project.conf";

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-f <config_path>] [-i <input_dir>] [-o <output_base>] <file_number>\n", prog);
    fprintf(stderr, "  -f: config file path (default: %s)\n", DEFAULT_CONFIG_PATH);
    fprintf(stderr, "  -i: override phase2_input_dir\n");
    fprintf(stderr, "  -o: override phase2_output_dir (base directory)\n");
}

int init_reg_state_slot(void)
{
    // 存 before/after 两份寄存器
    reg_state_base_slot = malloc(2 * sizeof(RegisterStates));
    if (!reg_state_base_slot) {
        perror("malloc reg_state_base_slot");
        return -1;
    }
    memset(reg_state_base_slot, 0, 2 * sizeof(RegisterStates));
    return 0;
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

    // RegisterStates *states = malloc(2 * sizeof(RegisterStates));
    // if (!states) {
    //     perror("malloc");
    //     return 1;
    // }

    // memset(states, 0, sizeof(RegisterStates) * 2);

    if (init_reg_state_slot() != 0) return 1;

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
    build_subdir(out_dir, sizeof(out_dir), output_base, "arithmetic_results");
    if (mkdir_p(out_dir, 0755) != 0) {
        perror("mkdir_p arithmetic_results");
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

    char cpsr_log_filename[768];
    snprintf(cpsr_log_filename, sizeof(cpsr_log_filename),
             "%s/res%d_cpsr.bin", out_dir, file_number);

    FILE *cpsr_log_file = fopen(cpsr_log_filename, "wb");

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

        current_range_index++;

        RegEffectBitmap rb;
        if (reg_bitmap_init(&rb, range_start, range_end) != 0) {
            fprintf(stderr, "[res%d] reg_bitmap_init failed for [%u, %u)\n",
                    file_number, range_start, range_end);
            continue;
        }
        
        for (uint32_t insn = range_start; insn < range_end; ++insn) {
            uint8_t insn_bytes[4];
            size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
            execute_insn_page_reg(insn_bytes, buf_length, NULL);
            const RegisterStates *b = &reg_state_base_slot[0];
            const RegisterStates *a = &reg_state_base_slot[1];
    
            bool gpr_changed =
                (b->r0  != a->r0 ) || (b->r1  != a->r1 ) || (b->r2  != a->r2 ) ||
                (b->r3  != a->r3 ) || (b->r4  != a->r4 ) || (b->r5  != a->r5 ) ||
                (b->r6  != a->r6 ) || (b->r7  != a->r7 ) || (b->r8  != a->r8 ) ||
                (b->r9  != a->r9 ) || (b->r10 != a->r10) || (b->r11 != a->r11) ||
                (b->r12 != a->r12);
        
            bool cpsr_changed = (b->cpsr != a->cpsr);
        
            bool all_gpr_zero = (b->r0 == 0) && (b->r1 == 0) && (b->r2 == 0) &&
                                (b->r3 == 0) && (b->r4 == 0) && (b->r5 == 0) &&
                                (b->r6 == 0) && (b->r7 == 0) && (b->r8 == 0) &&
                                (b->r9 == 0) && (b->r10 == 0) && (b->r11 == 0) &&
                                (b->r12 == 0);

            if (gpr_changed) {
                // printf("Instruction: 0x%x caused GPR change:\n", insn);
                reg_bitmap_mark_gpr(&rb, insn);
            } 
            if (cpsr_changed) {
                // printf("Instruction: 0x%x caused CPSR change:\n", insn);
                reg_bitmap_mark_cpsr(&rb, insn);

                if (cpsr_log_file) {
                    CpsrChangeLog log_entry;
                    log_entry.insn = insn;
                    log_entry.before = b->cpsr;
                    log_entry.after = a->cpsr;
                    fwrite(&log_entry, sizeof(CpsrChangeLog), 1, cpsr_log_file);
                }
            }
            if (all_gpr_zero) {
                reg_bitmap_mark_sp(&rb, insn);
            }
            
        }

        if (reg_bitmap_flush(&rb, output_file) != 0) {
            fprintf(stderr, "Failed to flush bitmap for range [0x%x, 0x%x]\n", 
                    range_start, range_end);
        }

        reg_bitmap_destroy(&rb);
    }

    if (cpsr_log_file) {
        fclose(cpsr_log_file);
    }
    // printf("\n");
    return 0;
}