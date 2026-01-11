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
    
    // if (last_insn_signum != 0) {
    //     // Optional: Log crash
    //     return; 
    // }

    // 3. Compare Results
    const RegisterStates *b = &reg_state_base_slot[0];
    const RegisterStates *a = &reg_state_base_slot[1];

    // printf("========== Register State Dump ==========\n");
    // printf("Reg   | Before (b) | After (a)  | Change\n");
    // printf("------|------------|------------|-------\n");
    // printf("R0    | 0x%08X | 0x%08X | %s\n", b->r0,  a->r0,  (b->r0  != a->r0)  ? "*" : "");
    // printf("R1    | 0x%08X | 0x%08X | %s\n", b->r1,  a->r1,  (b->r1  != a->r1)  ? "*" : "");
    // printf("R2    | 0x%08X | 0x%08X | %s\n", b->r2,  a->r2,  (b->r2  != a->r2)  ? "*" : "");
    // printf("R3    | 0x%08X | 0x%08X | %s\n", b->r3,  a->r3,  (b->r3  != a->r3)  ? "*" : "");
    // printf("R4    | 0x%08X | 0x%08X | %s\n", b->r4,  a->r4,  (b->r4  != a->r4)  ? "*" : "");
    // printf("R5    | 0x%08X | 0x%08X | %s\n", b->r5,  a->r5,  (b->r5  != a->r5)  ? "*" : "");
    // printf("R6    | 0x%08X | 0x%08X | %s\n", b->r6,  a->r6,  (b->r6  != a->r6)  ? "*" : "");
    // printf("R7    | 0x%08X | 0x%08X | %s\n", b->r7,  a->r7,  (b->r7  != a->r7)  ? "*" : "");
    // printf("R8    | 0x%08X | 0x%08X | %s\n", b->r8,  a->r8,  (b->r8  != a->r8)  ? "*" : "");
    // printf("R9    | 0x%08X | 0x%08X | %s\n", b->r9,  a->r9,  (b->r9  != a->r9)  ? "*" : "");
    // printf("R10   | 0x%08X | 0x%08X | %s\n", b->r10, a->r10, (b->r10 != a->r10) ? "*" : "");
    // printf("R11   | 0x%08X | 0x%08X | %s\n", b->r11, a->r11, (b->r11 != a->r11) ? "*" : "");
    // printf("R12   | 0x%08X | 0x%08X | %s\n", b->r12, a->r12, (b->r12 != a->r12) ? "*" : "");
    // printf("CPSR  | 0x%08X | 0x%08X | %s\n", b->cpsr, a->cpsr, (b->cpsr != a->cpsr) ? "*" : "");
    // printf("=========================================\n");

// --- [Debug Start] Register & CPSR Analysis (含Thumb检测) ---
{
    int i;
    const uint32_t *old_gpr = (const uint32_t *)&b->r0;
    const uint32_t *new_gpr = (const uint32_t *)&a->r0;
    printf("test instruction: 0x%x\n", insn);
    // 1. 打印通用寄存器 (GPR) 变化
    for (i = 0; i <= 12; i++) {
        if (old_gpr[i] != new_gpr[i]) {
            printf("[RegDiff] R%-2d: 0x%08X -> 0x%08X\n", i, old_gpr[i], new_gpr[i]);
        }
    }

    // 2. 重点分析 CPSR (状态寄存器)
    if (b->cpsr != a->cpsr) {
        uint32_t old_c = b->cpsr;
        uint32_t new_c = a->cpsr;
        uint32_t diff  = old_c ^ new_c; // 异或找出变化的位

        printf("[RegDiff] *** CPSR Changed ***: 0x%08X -> 0x%08X\n", old_c, new_c);

        // --- [新增] Thumb 状态检测 (Bit 5: T) ---
        if (diff & (1 << 5)) {
            int is_thumb = (new_c & (1 << 5));
            printf("    [ISA]  Instruction Set Change: %s -> %s\n", 
                   is_thumb ? "ARM" : "THUMB",   // 旧状态
                   is_thumb ? "THUMB" : "ARM");  // 新状态
        }

        // --- 中断控制位分析 (Bit 7: I, Bit 6: F) ---
        if (diff & (1 << 7)) {
            int irq_masked = (new_c & (1 << 7));
            printf("    [INT]  IRQ (I-bit) change: %s\n", 
                   irq_masked ? "MASKED (Disable)" : "UNMASKED (Enable)");
        }
        if (diff & (1 << 6)) {
            int fiq_masked = (new_c & (1 << 6));
            printf("    [INT]  FIQ (F-bit) change: %s\n", 
                   fiq_masked ? "MASKED (Disable)" : "UNMASKED (Enable)");
        }

        // --- 处理器模式分析 (Mode Bits 4:0) ---
        if (diff & 0x1F) {
            uint32_t old_mode = old_c & 0x1F;
            uint32_t new_mode = new_c & 0x1F;
            const char *mode_str = "UNKNOWN";

            switch(new_mode) {
                case 0x10: mode_str = "USR (EL0)"; break;
                case 0x11: mode_str = "FIQ (EL1)"; break;
                case 0x12: mode_str = "IRQ (EL1)"; break;
                case 0x13: mode_str = "SVC (EL1)"; break;
                case 0x17: mode_str = "ABT (EL1)"; break;
                case 0x1B: mode_str = "UND (EL1)"; break;
                case 0x1F: mode_str = "SYS (EL1)"; break;
                default:   mode_str = "OTHER";     break;
            }

            printf("    [MODE] CPU Mode Switch: 0x%02X -> 0x%02X [%s]\n", old_mode, new_mode, mode_str);

            if (old_mode == 0x10 && new_mode != 0x10) {
                printf("    >>>> ENTER KERNEL (EL0 -> EL1) <<<<\n");
            } else if (old_mode != 0x10 && new_mode == 0x10) {
                printf("    <<<< EXIT KERNEL  (EL1 -> EL0) <<<<\n");
            }
        }
    }
}
// --- [Debug End] ---

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
