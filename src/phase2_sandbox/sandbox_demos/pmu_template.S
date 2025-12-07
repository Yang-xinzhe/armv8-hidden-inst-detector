    .syntax unified
    .cpu cortex-a53
    .text
    .align 2

    /* 原型：
     *   void pmu_template(PmuResult *res);
     *
     * 约定：
     *   r0 = res (指向包含 ld_result / st_result 的结构体)
     *   r1, r2, r3 作为临时寄存器使用
     *
     * 要求：
     *   - 内核模块已在 EL1 写过 pmuserenr_el0 打开 EL0 PMU 访问
     *   - C 侧已调用 pmu_init() 配置：
     *       counter0 -> event 0x06 (LD_RETIRED)
     *       counter1 -> event 0x07 (ST_RETIRED)
     */

    .global pmu_template
    .type   pmu_template, %function
pmu_template:
    /* -------------------------------------------------
     * 1. 禁用 counter0 / counter1，防止遗留状态
     * ------------------------------------------------- */
    mov     r3, #( (1 << 0) | (1 << 1) )   @ mask = 0x3
    mcr     p15, 0, r3, c9, c12, 2        @ PMCNTENCLR = 0x3
    isb

    /* -------------------------------------------------
     * 2. 清零 counter0
     * ------------------------------------------------- */
    mov     r2, #0
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 0 (选择 counter0)
    isb
    mcr     p15, 0, r2, c9, c13, 2        @ PMXEVCNTR(counter0) = 0

    /* -------------------------------------------------
     * 3. 清零 counter1
     * ------------------------------------------------- */
    mov     r2, #1
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 1 (选择 counter1)
    isb
    mov     r2, #0
    mcr     p15, 0, r2, c9, c13, 2        @ PMXEVCNTR(counter1) = 0

    /* -------------------------------------------------
     * 4. 启用 counter0 / counter1
     * ------------------------------------------------- */
    mcr     p15, 0, r3, c9, c12, 1        @ PMCNTENSET = 0x3 (开 c0/c1)
    isb

    /* =================================================
     * 5. 被测指令窗口：
     *    这里的 nop 是占位，将来可以 runtime patch
     * ================================================= */
pmu_test_insn:
    ldr     r2, [r0]                                      @ 占位指令 (4 bytes)

    isb

    /* -------------------------------------------------
     * 6. 立刻关闭 counter0 / counter1
     * ------------------------------------------------- */
    mcr     p15, 0, r3, c9, c12, 2        @ PMCNTENCLR = 0x3
    isb

    /* -------------------------------------------------
     * 7. 读取 counter0 (LD_RETIRED) -> res->ld_result
     * ------------------------------------------------- */
    mov     r2, #0
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 0
    isb
    mrc     p15, 0, r2, c9, c13, 2        @ r2 = PMXEVCNTR(counter0)
    str     r2, [r0]                      @ res->ld_result = r2

    /* -------------------------------------------------
     * 8. 读取 counter1 (ST_RETIRED) -> res->st_result
     * ------------------------------------------------- */
    mov     r2, #1
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 1
    isb
    mrc     p15, 0, r3, c9, c13, 2        @ r3 = PMXEVCNTR(counter1)
    str     r3, [r0, #4]                  @ res->st_result = r3

    bx      lr

    .size pmu_template, .-pmu_template
