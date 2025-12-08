// pmu_user.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/printk.h>

static void enable_pmu_el0_on_cpu(void *info)
{
    u64 val;

    /* 1) 允许 EL0 访问 PMU 寄存器: PMUSERENR_EL0
     * bit0: EN  - 允许 EL0 使用 PMU 相关指令
     * bit1: SW  - 允许软件增量
     * bit2: CR  - 允许 EL0 访问某些控制寄存器
     * bit3: ER  - 允许 EL0 访问 event counters
     */
    asm volatile("mrs %0, pmuserenr_el0" : "=r"(val));
    val |= 0xF;   // EN | SW | CR | ER
    asm volatile("msr pmuserenr_el0, %0" :: "r"(val));

    /* 2) 打开 PMU 总开关 + 重置 cycle counter（可按需调整） */
    asm volatile("mrs %0, pmcr_el0" : "=r"(val));
    // bit0: E (enable all counters)
    // bit1: P (reset all event counters)
    // bit2: C (reset cycle counter)
    val |= (1u << 0) | (1u << 1) | (1u << 2);
    asm volatile("msr pmcr_el0, %0" :: "r"(val));

    /* 3) 使能 cycle counter (bit31)，以及你想用的 event counter bits */
    val = (1u << 31);   // PMCCNTR_EL0
    asm volatile("msr pmcntenset_el0, %0" :: "r"(val));

    asm volatile("isb");
    pr_info("pmu_user: CPU%d pmu enabled for EL0\n", smp_processor_id());
}

static void disable_pmu_el0_on_cpu(void *info)
{
    u64 zero = 0;

    /* 粗暴一点：直接清空 PMUSERENR_EL0，让 EL0 不再能访问 */
    asm volatile("msr pmuserenr_el0, %0" :: "r"(zero));
    asm volatile("isb");
    pr_info("pmu_user: CPU%d pmu disabled for EL0\n", smp_processor_id());
}

static int __init pmu_user_init(void)
{
    pr_info("pmu_user: enabling PMU access from EL0 on all CPUs\n");
    on_each_cpu(enable_pmu_el0_on_cpu, NULL, 1);
    return 0;
}

static void __exit pmu_user_exit(void)
{
    pr_info("pmu_user: disabling PMU access from EL0 on all CPUs\n");
    on_each_cpu(disable_pmu_el0_on_cpu, NULL, 1);
}

module_init(pmu_user_init);
module_exit(pmu_user_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yang Xinzhe");
MODULE_DESCRIPTION("Enable EL0 access to PMU registers on ARMv8-A");
