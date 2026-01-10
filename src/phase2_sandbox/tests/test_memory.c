#include "core.h"
#include "bitmap.h"
#include "sandbox.h"
#include "pmu_counter.h"
#include "test_runner.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

extern volatile sig_atomic_t last_insn_signum;

// ------------------------------------------------------------------
// Data Structures
// ------------------------------------------------------------------

typedef struct {
    RangeBitmap rb;
} MemEffectBitmap;

// ------------------------------------------------------------------
// Helper Functions
// ------------------------------------------------------------------

static void ensure_pmu_enabled() {
    int fd = open("/proc/pmu_user_enable", O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "1", 1) < 0) {
            // ignore error
        }
        close(fd);
    }
}

static void reg_bitmap_mark_ld(MemEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_LD, insn);
}

static void reg_bitmap_mark_st(MemEffectBitmap *rb, uint32_t insn)
{
    if (!rb) return;
    range_bitmap_mark(&rb->rb, RB_PLANE_ST, insn);
}

// ------------------------------------------------------------------
// TestOps Implementation
// ------------------------------------------------------------------

static int mem_global_init(void) {
    ensure_pmu_enabled();
    return 0;
}

static void* mem_create_bitmap(uint32_t start, uint32_t end, const char *out_dir, int file_number)
{
    (void)out_dir;
    (void)file_number;
    MemEffectBitmap *rb = malloc(sizeof(MemEffectBitmap));
    if (!rb) return NULL;
    
    // 使用 RB_MASK_LD | RB_MASK_ST 仅追踪访存指令
    if (range_bitmap_init_with_mask(&rb->rb, start, end, RB_MASK_LD | RB_MASK_ST) != 0) {
        free(rb);
        return NULL;
    }
    
    return rb;
}

static void mem_run_insn(uint32_t insn, void *bitmap_ptr)
{
    MemEffectBitmap *rb = (MemEffectBitmap *)bitmap_ptr;
    
    // 重置 insn_page 和 PMU 状态
    // 注意：test_runner 里的循环不会每次都 init_insn_page，
    // 但 PMU 测试可能比较敏感，特别是如果指令会修改自身或状态。
    // 为了稳健性，我们在 run_insn 里 re-init，但这会非常慢。
    // 原来的 test_memory.c 就是这么做的 (munmap -> init -> pmu_init)。
    // 为了性能，我们应该尽量避免 re-mmap，除非必要。
    // 但这里涉及到 PMU 配置，如果指令修改了 sysregs 可能会乱。
    // 这里我们稍微优化一下：不 munmap，只做 pmu_init() 和 clear_cache (execute_insn_page_pmu 内部会做)。
    // 但是，如果 init_insn_page 失败怎么办？
    // 为了兼容原逻辑，我们保持 re-init 逻辑，虽然慢。
    
    munmap(insn_region, PAGE_SIZE * 3);
    if (init_insn_page() != 0) return;
    
    pmu_init();
    
    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
    PmuResult res = {0};
    
    last_insn_signum = 0;
    execute_insn_page_pmu(insn_bytes, buf_length, &res);

    if (last_insn_signum != 0) {
        // Signal caught, ignore PMU result (likely crash/invalid)
        return;
    }

    // 过滤掉同时产生 LD 和 ST 的情况（通常是异常或干扰）
    if (res.ld_result > 0 && res.st_result > 0) {
        return;
    }

    // 只有计数很小（例如 < 20）才认为是单条指令产生的有效事件
    // 防止计数器溢出或长时间运行产生的噪音
    if (res.ld_result > 0 && res.ld_result < 20) {
        reg_bitmap_mark_ld(rb, insn);
    } 
    if (res.st_result > 0 && res.st_result < 20) {
        reg_bitmap_mark_st(rb, insn);
    }
}

static int mem_flush_bitmap(void *bitmap_ptr, FILE *file)
{
    MemEffectBitmap *rb = (MemEffectBitmap *)bitmap_ptr;
    if (!rb || !file) return -1;
    
    // 使用 bitmap.c 提供的序列化函数
    return range_bitmap_serialize(&rb->rb, file);
}

static void mem_destroy_bitmap(void *bitmap_ptr)
{
    MemEffectBitmap *rb = (MemEffectBitmap *)bitmap_ptr;
    if (rb) {
        range_bitmap_destroy(&rb->rb);
        free(rb);
    }
}

// ------------------------------------------------------------------
// Main Entry
// ------------------------------------------------------------------

static const TestOps mem_ops = {
    .test_name = "memaccess_results",
    .global_init = mem_global_init,
    .create_bitmap = mem_create_bitmap,
    .run_insn = mem_run_insn,
    .flush_bitmap = mem_flush_bitmap,
    .destroy_bitmap = mem_destroy_bitmap
};

int main(int argc, char *argv[]) {
    return run_test_framework(argc, argv, &mem_ops);
}
