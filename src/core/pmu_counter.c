#include "pmu_counter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Helper to get CPU info from /proc/cpuinfo */
static void get_cpu_info(uint32_t *implementer, uint32_t *part) {
    *implementer = 0;
    *part = 0;

    int cpu_id = sched_getcpu();
    if (cpu_id < 0) cpu_id = 0;

    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;

    char line[256];
    int current_processor = -1;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "processor", 9) == 0) {
            char *p = strchr(line, ':');
            if (p) {
                current_processor = atoi(p + 1);
            }
        }

        if (current_processor == cpu_id) {
            if (strstr(line, "CPU implementer")) {
                char *p = strchr(line, ':');
                if (p) {
                    *implementer = (uint32_t)strtoul(p + 1, NULL, 0);
                }
            }
            if (strstr(line, "CPU part")) {
                char *p = strchr(line, ':');
                if (p) {
                    *part = (uint32_t)strtoul(p + 1, NULL, 0);
                }
            }
        }

        if (*implementer != 0 && *part != 0) break;
    }
    fclose(f);
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

    /* 3) 识别 CPU 并选择 Event */
    uint32_t implementer = 0;
    uint32_t part_num = 0;
    get_cpu_info(&implementer, &part_num);

    // printf("[PMU Init] /proc/cpuinfo -> Impl=0x%x, Part=0x%x\n", implementer, part_num);

    /* 默认使用 A53/A55 的 Retired 事件 */
    uint32_t ld_evt = 0x06; // LD_RETIRED
    uint32_t st_evt = 0x07; // ST_RETIRED

    if (implementer == 0x41) { // ARM Limited
        if (part_num == 0xD08) {
            /* Cortex-A72 (0xD08): 使用 SPEC 事件 */
            ld_evt = 0x70; // LD_SPEC
            st_evt = 0x71; // ST_SPEC
        }
    } else if (implementer == 0x4E) { // Nvidia
        /* Nvidia Carmel 等核心 */
        // printf("[PMU Warning] Nvidia CPU detected. Using default 0x06/0x07.\n");
        ld_evt = 0x70; // LD_SPEC
        st_evt = 0x71; // ST_SPEC
    }

    /* counter0 -> Load Event */
    write_pmselr(0);
    write_pmxevtyper(ld_evt);

    /* counter1 -> Store Event */
    write_pmselr(1);
    write_pmxevtyper(st_evt);

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
}
