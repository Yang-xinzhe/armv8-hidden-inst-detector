#include "core.h"
#include "sandbox.h"
#include "register_states.h"
#include "bitmap.h"
#include "config.h"
#include "fs_utils.h"

typedef struct {
    uint32_t insn;
    uint32_t before;
    uint32_t after;
} FpsrChangeLog;

SimdRegisterStates *simd_state_base_slot = NULL;

typedef struct {
    RangeBitmap rb;
} SimdEffectBitmap;

static int reg_bitmap_init(SimdEffectBitmap *rb, uint32_t start, uint32_t end)
{
    if (!rb) return -1;
    return range_bitmap_init_with_mask(&rb->rb, start, end, RB_MASK_SIMD | RB_MASK_FPSCR);
}

static void reg_bitmap_mark_simd(SimdEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_SIMD, insn);
}

static void reg_bitmap_mark_fpsr(SimdEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_FPSCR, insn);
}

static int reg_bitmap_flush(SimdEffectBitmap *rb, FILE *file)
{
    if (!rb || !file) return -1;
    const RangeBitmap *r = &rb->rb;
    if (fwrite(&r->start, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->end,   sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->size,  sizeof(uint32_t), 1, file) != 1) return -1;

    if (!(r->plane_mask & RB_MASK_SIMD) || !r->planes[RB_PLANE_SIMD]) return -1;
    if (fwrite(r->planes[RB_PLANE_SIMD], 1, r->size, file) != r->size) return -1;

    if (!(r->plane_mask & RB_MASK_FPSCR) || !r->planes[RB_PLANE_FPSCR]) return -1;
    if (fwrite(r->planes[RB_PLANE_FPSCR], 1, r->size, file) != r->size) return -1;

    return 0;
}

static void reg_bitmap_destroy(SimdEffectBitmap *rb)
{
    if (!rb) return;
    range_bitmap_destroy(&rb->rb);
}

int init_simd_state_slot(void)
{
    simd_state_base_slot = aligned_alloc(16, 2 * sizeof(SimdRegisterStates));
    if (!simd_state_base_slot) {
        perror("aligned_alloc");
        return -1;
    }
    memset(simd_state_base_slot, 0, 2 * sizeof(SimdRegisterStates));
    return 0;
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

int main(int argc, const char* argv[]) {

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <file_number>\n", argv[0]);
        fprintf(stderr, "Example: %s 1  # Handling hidden_insn/res1.txt\n", argv[0]);
        return 1;
    }

    int target_file_num = atoi(argv[1]);
    int file_number = target_file_num;

    ProjectConfig cfg;
    project_config_init(&cfg);
    (void)project_config_load(&cfg, "config/project.conf");
    const char *input_dir = cfg.phase2_input_dir;
    const char *output_base = cfg.phase2_output_dir;

    if (init_simd_state_slot() != 0) {
        return 1;
    }
    
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
    build_subdir(out_dir, sizeof(out_dir), output_base, "simd_results");
    if (mkdir_p(out_dir, 0755) != 0) {
        perror("mkdir_p simd_results");
        return 1;
    }

    char output_filename[256];
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

    char cpsr_log_filename[256];
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

        SimdEffectBitmap rb;
        if (reg_bitmap_init(&rb, range_start, range_end) != 0) {
            fprintf(stderr, "[res%d] reg_bitmap_init failed for [%u, %u)\n",
                    file_number, range_start, range_end);
            continue;
        }
        
        for (uint32_t insn = range_start ; insn < range_end ; ++insn) {
            uint8_t insn_bytes[4];
            size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
            execute_insn_page_simd(insn_bytes, buf_length, simd_state_base_slot);

            if (last_insn_signum != 0) {
                continue;
            }

            const SimdRegisterStates *b = &simd_state_base_slot[0];
            const SimdRegisterStates *a = &simd_state_base_slot[1];
    
            bool simd_changed = false;
            for (int i = 0; i < 16; ++i) {
                if (memcmp(b->q[i], a->q[i], 16) != 0) {
                    simd_changed = true;
                    break;
                }
            }

            bool fpsr_changed = (b->fpscr != a->fpscr);

            if (simd_changed) {
                // printf("Instruction: 0x%x caused SIMD change:\n", test_simd_insns[insn]);
                reg_bitmap_mark_simd(&rb, insn);
            } 
            if (fpsr_changed) {
                // printf("Instruction: 0x%x caused FPSCR change:\n", test_simd_insns[insn]);
                reg_bitmap_mark_fpsr(&rb, insn);

                if (cpsr_log_file) {
                    FpsrChangeLog log_entry;
                    log_entry.insn = insn;
                    log_entry.before = b->fpscr;
                    log_entry.after = a->fpscr;
                    fwrite(&log_entry, sizeof(FpsrChangeLog), 1, cpsr_log_file);
                }
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
    return 0;
}
