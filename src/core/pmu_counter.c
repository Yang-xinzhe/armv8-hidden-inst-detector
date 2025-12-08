#include "pmu_counter.h"

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

void pmu_init(void)
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