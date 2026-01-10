#include "core.h"
#include "sandbox.h"
#include "register_states.h"
#include "bitmap.h"
#include "test_runner.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

// ------------------------------------------------------------------
// Data Structures
// ------------------------------------------------------------------

typedef struct {
    uint32_t insn;
    uint32_t before;
    uint32_t after;
} FpsrChangeLog;

// Global slot for saving SIMD register states
SimdRegisterStates *simd_state_base_slot = NULL;

typedef struct {
    RangeBitmap rb;
    FILE *fpsr_log_file;
} SimdEffectBitmap;

extern volatile sig_atomic_t last_insn_signum;

// ------------------------------------------------------------------
// Helper Functions
// ------------------------------------------------------------------

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

// ------------------------------------------------------------------
// TestOps Implementation
// ------------------------------------------------------------------

static int simd_global_init(void)
{
    // Must use aligned_alloc for SIMD loads/stores
    simd_state_base_slot = aligned_alloc(16, 2 * sizeof(SimdRegisterStates));
    if (!simd_state_base_slot) {
        perror("aligned_alloc simd_state_base_slot");
        return -1;
    }
    memset(simd_state_base_slot, 0, 2 * sizeof(SimdRegisterStates));
    return 0;
}

static void* simd_create_bitmap(uint32_t start, uint32_t end, const char *out_dir, int file_number)
{
    (void)out_dir;
    (void)file_number;
    SimdEffectBitmap *rb = malloc(sizeof(SimdEffectBitmap));
    if (!rb) return NULL;
    
    if (range_bitmap_init_with_mask(&rb->rb, start, end, RB_MASK_SIMD | RB_MASK_FPSCR) != 0) {
        free(rb);
        return NULL;
    }
    
    return rb;
}

static void simd_run_insn(uint32_t insn, void *bitmap_ptr)
{
    SimdEffectBitmap *rb = (SimdEffectBitmap *)bitmap_ptr;
    
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
    
    execute_insn_page_simd(insn_bytes, buf_length, simd_state_base_slot);

    if (last_insn_signum != 0) {
        return;
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
        reg_bitmap_mark_simd(rb, insn);
    } 
    if (fpsr_changed) {
        reg_bitmap_mark_fpsr(rb, insn);
        // Log FPSR change details if needed
    }
}

static int simd_flush_bitmap(void *bitmap_ptr, FILE *file)
{
    SimdEffectBitmap *rb = (SimdEffectBitmap *)bitmap_ptr;
    if (!rb || !file) return -1;
    
    const RangeBitmap *r = &rb->rb;
    
    // Header
    if (fwrite(&r->start, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->end,   sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&r->size,  sizeof(uint32_t), 1, file) != 1) return -1;

    // Planes
    if (fwrite(r->planes[RB_PLANE_SIMD], 1, r->size, file) != r->size) return -1;
    if (fwrite(r->planes[RB_PLANE_FPSCR], 1, r->size, file) != r->size) return -1;

    return 0;
}

static void simd_destroy_bitmap(void *bitmap_ptr)
{
    SimdEffectBitmap *rb = (SimdEffectBitmap *)bitmap_ptr;
    if (rb) {
        range_bitmap_destroy(&rb->rb);
        free(rb);
    }
}

// ------------------------------------------------------------------
// Main Entry
// ------------------------------------------------------------------

static const TestOps simd_ops = {
    .test_name = "simd_results",
    .global_init = simd_global_init,
    .create_bitmap = simd_create_bitmap,
    .run_insn = simd_run_insn,
    .flush_bitmap = simd_flush_bitmap,
    .destroy_bitmap = simd_destroy_bitmap
};

int main(int argc, char *argv[]) {
    return run_test_framework(argc, argv, &simd_ops);
}
