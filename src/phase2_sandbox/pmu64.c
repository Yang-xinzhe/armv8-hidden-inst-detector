#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * 简单 AArch64 PMU demo
 *  - counter0 统计 LD_RETIRED (0x06)
 *  - counter1 统计 ST_RETIRED (0x07)
 *  - cycle counter 统计周期数
 *
 * 编译（在 RK3399 上）：
 *     gcc -O2 pmu_demo.c -o pmu_demo
 * 运行：
 *     taskset -c 0 ./pmu_demo
 */

/* 读 / 写 64-bit 系统寄存器的封装 */
static inline uint64_t read_pmcr_el0(void)
{
    uint64_t v;
    asm volatile("mrs %0, pmcr_el0" : "=r"(v));
    return v;
}

static inline void write_pmcr_el0(uint64_t v)
{
    asm volatile("msr pmcr_el0, %0" :: "r"(v));
}

static inline uint64_t read_pmcntenset_el0(void)
{
    uint64_t v;
    asm volatile("mrs %0, pmcntenset_el0" : "=r"(v));
    return v;
}

static inline void write_pmcntenset_el0(uint64_t v)
{
    asm volatile("msr pmcntenset_el0, %0" :: "r"(v));
}

static inline void write_pmcntenclr_el0(uint64_t v)
{
    asm volatile("msr pmcntenclr_el0, %0" :: "r"(v));
}

static inline void write_pmevtyper0_el0(uint64_t v)
{
    asm volatile("msr pmevtyper0_el0, %0" :: "r"(v));
}

static inline void write_pmevtyper1_el0(uint64_t v)
{
    asm volatile("msr pmevtyper1_el0, %0" :: "r"(v));
}

static inline uint64_t read_pmevcntr0_el0(void)
{
    uint64_t v;
    asm volatile("mrs %0, pmevcntr0_el0" : "=r"(v));
    return v;
}

static inline uint64_t read_pmevcntr1_el0(void)
{
    uint64_t v;
    asm volatile("mrs %0, pmevcntr1_el0" : "=r"(v));
    return v;
}

static inline void write_pmevcntr0_el0(uint64_t v)
{
    asm volatile("msr pmevcntr0_el0, %0" :: "r"(v));
}

static inline void write_pmevcntr1_el0(uint64_t v)
{
    asm volatile("msr pmevcntr1_el0, %0" :: "r"(v));
}

static inline uint64_t read_pmccntr_el0(void)
{
    uint64_t v;
    asm volatile("mrs %0, pmccntr_el0" : "=r"(v));
    return v;
}

static inline void write_pmccntr_el0(uint64_t v)
{
    asm volatile("msr pmccntr_el0, %0" :: "r"(v));
}

/* 初始化 PMU：启用、清零计数器、配置 event type */
static void pmu_init(void)
{
    uint64_t v;

    /* 1) 先关掉所有计数器，防止遗留状态 */
    write_pmcntenclr_el0(~0ull);

    /* 2) 配置 PMCR_EL0:
     *    bit0 E = 1  启用所有计数器
     *    bit1 P = 1  复位 event counters
     *    bit2 C = 1  复位 cycle counter
     *
     *  P、C 是写 1 即触发复位，写完会自动清零。
     */
    v = read_pmcr_el0();
    v |= (1u << 0);  // E
    v |= (1u << 1);  // P: reset event counters
    v |= (1u << 2);  // C: reset cycle counter
    write_pmcr_el0(v);

    /* 3) 配置 event type：
     *    counter0: LD_RETIRED = 0x06
     *    counter1: ST_RETIRED = 0x07
     */
    write_pmevtyper0_el0(0x06); // LD_RETIRED
    write_pmevtyper1_el0(0x07); // ST_RETIRED

    /* 4) 清零各自的 counter 值 */
    write_pmevcntr0_el0(0);
    write_pmevcntr1_el0(0);
    write_pmccntr_el0(0);

    /* 5) 使能 counter0、counter1 和 cycle counter
     *    bit0 -> PMEVCNTR0
     *    bit1 -> PMEVCNTR1
     *    bit31 -> PMCCNTR (cycle counter)
     */
    uint64_t mask = (1ull << 0) | (1ull << 1) | (1ull << 31);
    write_pmcntenset_el0(mask);

    /* 打印当前 PMCNTENSET_EL0 状态 */
    uint64_t cnt_en = read_pmcntenset_el0();
    printf("PMCNTENSET_EL0 after init: 0x%016llx\n",
           (unsigned long long)cnt_en);
}

/* 一个简单的 load/store 压测，用来产生事件 */
static void workload(void)
{
    enum { N = 1024 };
    static int arr[N];

    for (int i = 0; i < N; i++) {
        arr[i] = i;
    }

    volatile uint64_t sum = 0;

    for (int iter = 0; iter < 10000; iter++) {
        for (int i = 0; i < N; i++) {
            int tmp = arr[i];   // load
            sum += tmp;         // 使用，防止被优化
            arr[i] = tmp + 1;   // store
        }
    }

    /* 防止编译器把整个循环优化掉 */
    if (sum == 0xdeadbeef)
        printf("sum = %llu\n", (unsigned long long)sum);
}

int main(void)
{
    printf("PMU demo start.\n");

    pmu_init();

    uint64_t ld_before  = read_pmevcntr0_el0();
    uint64_t st_before  = read_pmevcntr1_el0();
    uint64_t cy_before  = read_pmccntr_el0();

 

    // workload();
    __asm__ __volatile__("LDR X0, [X1, #0] ");

    uint64_t ld_after  = read_pmevcntr0_el0();
    uint64_t st_after  = read_pmevcntr1_el0();
    uint64_t cy_after  = read_pmccntr_el0();

    printf("Before workload:\n");
    printf("  LD_RETIRED  = %llu\n", (unsigned long long)ld_before);
    printf("  ST_RETIRED  = %llu\n", (unsigned long long)st_before);
    printf("  CYCLES      = %llu\n", (unsigned long long)cy_before);

    printf("After workload:\n");
    printf("  LD_RETIRED  = %llu (delta = %llu)\n",
           (unsigned long long)ld_after,
           (unsigned long long)(ld_after - ld_before));

    printf("  ST_RETIRED  = %llu (delta = %llu)\n",
           (unsigned long long)st_after,
           (unsigned long long)(st_after - st_before));

    printf("  CYCLES      = %llu (delta = %llu)\n",
           (unsigned long long)cy_after,
           (unsigned long long)(cy_after - cy_before));

    /* 结束时关掉计数器（可选） */
    write_pmcntenclr_el0(~0ull);

    return 0;
}
