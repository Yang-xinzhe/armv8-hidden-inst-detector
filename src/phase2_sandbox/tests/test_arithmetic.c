#include "core.h"
#include "register_states.h"
#include "bitmap.h"
#include "sandbox.h"
#include "test_runner.h"

#include <string.h>
#include <unistd.h>

// ------------------------------------------------------------------
// Data Structures
// ------------------------------------------------------------------

typedef struct {
    uint32_t insn;
    uint32_t before;
    uint32_t after;
} CpsrChangeLog;

// Global slot for saving register states (allocated in init)
RegisterStates *reg_state_base_slot = NULL; 

typedef struct {
    RangeBitmap rb;
    FILE *cpsr_log_file; // Log file for this range (opened in create_bitmap)
} RegEffectBitmap;

// ------------------------------------------------------------------
// Helper Functions
// ------------------------------------------------------------------

static int init_reg_state_slot(void)
{
    reg_state_base_slot = malloc(2 * sizeof(RegisterStates));
    if (!reg_state_base_slot) {
        perror("malloc reg_state_base_slot");
        return -1;
    }
    memset(reg_state_base_slot, 0, 2 * sizeof(RegisterStates));
    return 0;
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

// ------------------------------------------------------------------
// TestOps Implementation
// ------------------------------------------------------------------

static void* arith_create_bitmap(uint32_t start, uint32_t end, const char *out_dir, int file_number)
{
    RegEffectBitmap *rb = malloc(sizeof(RegEffectBitmap));
    if (!rb) return NULL;
    
    if (range_bitmap_init_with_mask(&rb->rb, start, end, RB_MASK_GPR | RB_MASK_CPSR | RB_MASK_SP) != 0) {
        free(rb);
        return NULL;
    }
    
    // Create CPSR log file: res{N}_cpsr.bin
    // Note: If multiple ranges exist in one file, we should probably append or use unique names?
    // The original code used one cpsr file per input file (res%d.txt -> res%d_cpsr.bin).
    // Here we are called per range. If we open "w", we overwrite previous ranges!
    // Solution: Open in "a" (append) mode. But runner creates output dir fresh?
    // Wait, runner creates `res%d_complete.bin`. 
    // We should use `res%d_cpsr.bin`.
    
    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/res%d_cpsr.bin", out_dir, file_number);
    
    // Use "ab" to append if file exists (multiple ranges in one file)
    // But for the first range, we might want "wb"? 
    // Since runner processes ranges sequentially, "ab" is safe if we ensure it's clean initially.
    // Or just always use "ab".
    
    rb->cpsr_log_file = fopen(log_path, "ab");
    if (!rb->cpsr_log_file) {
        // Warn but continue?
        fprintf(stderr, "Warning: Failed to open CPSR log %s\n", log_path);
    }
    
    return rb;
}

static void arith_run_insn(uint32_t insn, void *bitmap_ptr)
{
    RegEffectBitmap *rb = (RegEffectBitmap *)bitmap_ptr;
    
    // 1. Fill instruction buffer
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
    
    // 2. Execute
    execute_insn_page_reg(insn_bytes, buf_length, NULL);
    
    // 3. Compare Results
    const RegisterStates *b = &reg_state_base_slot[0];
    const RegisterStates *a = &reg_state_base_slot[1];
    size_t count = sizeof(RegisterStates) / sizeof(uint32_t);
    uint32_t *ptr = (uint32_t *)b;
    printf("Before:\n");
    for(size_t i = 0 ; i < count; ++i) {
        if (i < 13) {
            printf("r%zu: 0x%08x\n", i, ptr[i]);
        } else {
            printf("cpsr: 0x%08x\n", ptr[i]);
        }
    }
    *ptr = (uint32_t *)a;
    printf("After:\n");
    for(size_t i = 0 ; i < count; ++i) {
        if (i < 13) {
            printf("r%zu: 0x%08x\n", i, ptr[i]);
        } else {
            printf("cpsr: 0x%08x\n", ptr[i]);
        }
    }

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
        reg_bitmap_mark_gpr(rb, insn);
    } 
    if (cpsr_changed) {
        reg_bitmap_mark_cpsr(rb, insn);
        
        // Log details
        if (rb->cpsr_log_file) {
            CpsrChangeLog log_entry;
            log_entry.insn = insn;
            log_entry.before = b->cpsr;
            log_entry.after = a->cpsr;
            fwrite(&log_entry, sizeof(CpsrChangeLog), 1, rb->cpsr_log_file);
        }
    }
    if (all_gpr_zero) {
        reg_bitmap_mark_sp(rb, insn);
    }
}

static int arith_flush_bitmap(void *bitmap_ptr, FILE *file)
{
    RegEffectBitmap *rb = (RegEffectBitmap *)bitmap_ptr;
    if (!rb || !file) return -1;
    
    const RangeBitmap *r = &rb->rb;
    
    // Write Header
    if (fwrite(&r->start, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->end,   sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->size,  sizeof(uint32_t), 1, file) != 1) return -1;

    // Write Planes
    if (fwrite(r->planes[RB_PLANE_GPR], 1, r->size, file) != r->size) return -1;
    if (fwrite(r->planes[RB_PLANE_CPSR], 1, r->size, file) != r->size) return -1;
    if (fwrite(r->planes[RB_PLANE_SP], 1, r->size, file) != r->size) return -1;

    // Flush Log File as well
    if (rb->cpsr_log_file) {
        fflush(rb->cpsr_log_file);
    }

    return 0;
}

static void arith_destroy_bitmap(void *bitmap_ptr)
{
    RegEffectBitmap *rb = (RegEffectBitmap *)bitmap_ptr;
    if (rb) {
        if (rb->cpsr_log_file) {
            fclose(rb->cpsr_log_file);
        }
        range_bitmap_destroy(&rb->rb);
        free(rb);
    }
}

// ------------------------------------------------------------------
// Main Entry
// ------------------------------------------------------------------

static const TestOps arith_ops = {
    .test_name = "arithmetic_results",
    .global_init = init_reg_state_slot,
    .create_bitmap = arith_create_bitmap,
    .run_insn = arith_run_insn,
    .flush_bitmap = arith_flush_bitmap,
    .destroy_bitmap = arith_destroy_bitmap
};

int main(int argc, char *argv[]) {
    return run_test_framework(argc, argv, &arith_ops);
}
