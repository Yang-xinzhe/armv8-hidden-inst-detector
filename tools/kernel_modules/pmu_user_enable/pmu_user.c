// pmu_user.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROC_FILENAME "pmu_user_enable"

static void enable_pmu_el0_on_cpu(void *info)
{
    u64 val;

    /* 1) 允许 EL0 访问 PMU 寄存器: PMUSERENR_EL0 */
    asm volatile("mrs %0, pmuserenr_el0" : "=r"(val));
    val |= 0xF;   // EN | SW | CR | ER
    asm volatile("msr pmuserenr_el0, %0" :: "r"(val));

    /* 2) 打开 PMU 总开关 + 重置 cycle counter */
    asm volatile("mrs %0, pmcr_el0" : "=r"(val));
    val |= (1u << 0) | (1u << 1) | (1u << 2);
    asm volatile("msr pmcr_el0, %0" :: "r"(val));

    /* 3) 使能 cycle counter */
    val = (1u << 31);
    asm volatile("msr pmcntenset_el0, %0" :: "r"(val));

    asm volatile("isb");
    // pr_info("pmu_user: CPU%d pmu enabled for EL0\n", smp_processor_id());
}

static void disable_pmu_el0_on_cpu(void *info)
{
    u64 zero = 0;
    asm volatile("msr pmuserenr_el0, %0" :: "r"(zero));
    asm volatile("isb");
}

// Write handler for /proc/pmu_user_enable
static ssize_t pmu_enable_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) 
{
    // 当用户写入任何内容时，强制在所有 CPU 上再次开启
    on_each_cpu(enable_pmu_el0_on_cpu, NULL, 1);
    return count;
}

static const struct proc_ops pmu_proc_ops = {
    .proc_write = pmu_enable_write,
};

static struct proc_dir_entry *pmu_proc_entry;

static int __init pmu_user_init(void)
{
    pr_info("pmu_user: init. creating /proc/%s\n", PROC_FILENAME);
    
    // 初始加载时也尝试开启一次
    on_each_cpu(enable_pmu_el0_on_cpu, NULL, 1);

    pmu_proc_entry = proc_create(PROC_FILENAME, 0666, NULL, &pmu_proc_ops);
    if (!pmu_proc_entry) {
        pr_err("pmu_user: failed to create proc entry\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit pmu_user_exit(void)
{
    if (pmu_proc_entry) {
        proc_remove(pmu_proc_entry);
    }
    pr_info("pmu_user: disabling PMU access from EL0 on all CPUs\n");
    on_each_cpu(disable_pmu_el0_on_cpu, NULL, 1);
}

module_init(pmu_user_init);
module_exit(pmu_user_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yang Xinzhe");
MODULE_DESCRIPTION("Enable EL0 access to PMU registers on ARMv8-A with proc trigger");
