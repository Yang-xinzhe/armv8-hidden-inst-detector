#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * AArch32 PMU demo
 *  - counter0 统计 LD_RETIRED (0x06)
 *  - counter1 统计 ST_RETIRED (0x07)
 *  - cycle counter 统计周期数
 *
 * 编译（在上位机）：
 *   arm-linux-gnueabihf-gcc -marm -march=armv8-a -O2 pmu32.c -o pmu32
 *
 * 在 RK3399 上运行：
 *   taskset -c 0 ./pmu32
 *
 * 注意：需要内核模块已在 EL1 中通过 pmuserenr_el0 允许 EL0 访问 PMU。
 */

/* ===== AArch32 下的 PMU 寄存器封装（CP15 方式） ===== */

/* PMCR: Performance Monitor Control Register, p15,0,c9,c12,0 */
static inline uint32_t read_pmcr(void)
{
    uint32_t v;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c12, 0" : "=r"(v));
    return v;
}

static inline void write_pmcr(uint32_t v)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c12, 0" :: "r"(v));
}

/* PMCNTENSET / PMCNTENCLR: 计数器使能 / 关闭 */
static inline uint32_t read_pmcntenset(void)
{
    uint32_t v;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c12, 1" : "=r"(v));
    return v;
}

static inline void write_pmcntenset(uint32_t v)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c12, 1" :: "r"(v));
}

static inline void write_pmcntenclr(uint32_t v)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c12, 2" :: "r"(v));
}

/* PMSELR: 选择哪个 event counter (0..N-1) */
static inline void write_pmselr(uint32_t idx)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c12, 5" :: "r"(idx));
    __asm__ __volatile__("isb");
}

/* PMXEVTYPER: 当前选中 event counter 的事件类型 */
static inline void write_pmxevtyper(uint32_t evt)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c13, 1" :: "r"(evt));
}

/* PMXEVCNTR: 当前选中 event counter 的计数值 */
static inline void write_pmxevcntr(uint32_t v)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c13, 2" :: "r"(v));
}

static inline uint32_t read_pmxevcntr(void)
{
    uint32_t v;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 2" : "=r"(v));
    return v;
}

/* 读指定 index 的 event counter （模拟 pmevcntrN_el0） */
static inline uint32_t read_pmevcntrN(uint32_t idx)
{
    write_pmselr(idx);
    return read_pmxevcntr();
}

/* 写指定 index 的 event counter */
static inline void write_pmevcntrN(uint32_t idx, uint32_t v)
{
    write_pmselr(idx);
    write_pmxevcntr(v);
}

/* PMCCNTR: cycle counter (用 32bit 即可) */
static inline uint32_t read_pmccntr(void)
{
    uint32_t v;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(v));
    return v;
}

static inline void write_pmccntr(uint32_t v)
{
    __asm__ __volatile__("mcr p15, 0, %0, c9, c13, 0" :: "r"(v));
}

/* ===== 初始化 PMU：启用、清零计数器、配置 event type ===== */

static void pmu_init(void)
{
    uint32_t v;

    /* 1) 先关掉所有计数器，防止遗留状态 */
    write_pmcntenclr(0xFFFFFFFFu);

    /* 2) 配置 PMCR:
     *    bit0 E = 1  启用所有计数器
     *    bit1 P = 1  复位 event counters
     *    bit2 C = 1  复位 cycle counter
     */
    v = read_pmcr();
    v |= (1u << 0);  // E
    v |= (1u << 1);  // P: reset event counters
    v |= (1u << 2);  // C: reset cycle counter
    write_pmcr(v);

    /* 3) 配置 event type：
     *    counter0: LD_RETIRED = 0x06
     *    counter1: ST_RETIRED = 0x07
     *    通过 PMSELR + PMXEVTYPER 来设置
     */

    /* counter0 -> LD_RETIRED (0x06) */
    write_pmselr(0);
    write_pmxevtyper(0x06);

    /* counter1 -> ST_RETIRED (0x07) */
    write_pmselr(1);
    write_pmxevtyper(0x07);

    /* 4) 清零各自的 counter 值 */
    write_pmevcntrN(0, 0);
    write_pmevcntrN(1, 0);
    write_pmccntr(0);

    /* 5) 使能 counter0、counter1 和 cycle counter
     *    bit0 -> counter0
     *    bit1 -> counter1
     *    bit31 -> cycle counter
     */
    uint32_t mask = (1u << 0) | (1u << 1) | (1u << 31);
    write_pmcntenset(mask);

    // /* 打印当前 PMCNTENSET 状态 */
    // uint32_t cnt_en = read_pmcntenset();
    // printf("PMCNTENSET after init: 0x%08x\n", cnt_en);
}

int main(void)
{
    printf("PMU demo (AArch32) start.\n");

    pmu_init();

    uint32_t ld_before  = read_pmevcntrN(0);
    uint32_t st_before  = read_pmevcntrN(1);
    // uint32_t cy_before  = read_pmccntr();

    /* 这里可以选择跑 workload，或者只测一条指令 */

    // workload();

    /* AArch32: 单条 LDR 指令测试版本
     * 注意：这里没有给 r1 提前赋一个有效地址，只是示例。
     * 真正使用时你要先在 C 里设置 r1 指向一块可访问内存：
     *
     *   int x = 0;
     *   int *p = &x;
     *   asm volatile("mov r1, %0" :: "r"(p) : "r1");
     *   asm volatile("ldr r0, [r1, #0]");
     */
    // __asm__ __volatile__("nop\n");
        // __asm__ __volatile__("ldr r0, [sp, #0]\n");
    int x = 0;
    int *p = &x;
    __asm__ __volatile__("mov r1, %0" :: "r"(p) : "r1");
// 然后在 asm block 里读写 [r1]


    __asm__ __volatile__(
        "mov r0, #3\n"                    /* bit0+bit1: counter0+1 */
        "mcr p15, 0, r0, c9, c12, 1\n"    /* PMCNTENSET = 0x3, 开 c0/c1 */
        "isb\n"

        /* ========= 在这里改你要测试的指令 ========= */
        "nop\n"
        /* 示例：
         *   "ldr r2, [r1, #0]\n"    // 期望: LD += 1, ST 不变
         *   "str r2, [r1, #0]\n"    // 期望: ST += 1, LD 不变
         */
        /* ======================================= */

        "isb\n"
        "mcr p15, 0, r0, c9, c12, 2\n"    /* PMCNTENCLR = 0x3, 关 c0/c1 */
        :
        :
        : "r0", "r2", "memory"
    );

    // __asm__ __volatile__("str  R0, [SP, #0]\n");

    uint32_t ld_after  = read_pmevcntrN(0);
    uint32_t st_after  = read_pmevcntrN(1);
    // uint32_t cy_after  = read_pmccntr();

    printf("Before workload / instruction:\n");
    printf("  LD_RETIRED  = %u\n", ld_before);
    printf("  ST_RETIRED  = %u\n", st_before);
    // printf("  CYCLES      = %u\n", cy_before);

    printf("After workload / instruction:\n");
    printf("  LD_RETIRED  = %u (delta = %u)\n",
           ld_after,  (unsigned)(ld_after  - ld_before));
    printf("  ST_RETIRED  = %u (delta = %u)\n",
           st_after,  (unsigned)(st_after  - st_before));
    // printf("  CYCLES      = %u (delta = %u)\n",
    //        cy_after,  (unsigned)(cy_after  - cy_before));

    /* 结束时关掉计数器（可选） */
    write_pmcntenclr(0xFFFFFFFFu);

    return 0;
}
