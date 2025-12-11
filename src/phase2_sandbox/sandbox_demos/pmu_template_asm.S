    .syntax unified
    .cpu cortex-a53
    .text
    .align 2

    /*
     * 原型（在 C 侧我们会当成 uint64_t (*)(void) 来调用）：
     *
     *   uint64_t pmu_template(void);
     *
     * 返回值：
     *   r0 = LD_RETIRED（低 32 bit）
     *   r1 = ST_RETIRED（高 32 bit）
     *
     * 约定：
     *   - 不使用 r4-r11（避免需要保存/恢复）
     *   - 只用 r0-r3, ip(r12) 作为 scratch
     *   - 不访问内存（不 push/pop，不 str/ldr，不 stm/ldm）
     *
     * 依赖：
     *   - 内核已打开 EL0 的 PMU 访问权限 (pmuserenr_el0.EN = 1)
     *   - 用户态已调用 pmu_init() 配置好：
     *       counter0 -> LD_RETIRED (0x06)
     *       counter1 -> ST_RETIRED (0x07)
     */

    .global pmu_template
    .global boilerplate_start
    .global insn_location
    .global boilerplate_end

    .type   pmu_template, %function
pmu_template:
boilerplate_start:
    /* r0-r3 都当作 scratch，用完就扔 */

    push    {r2-r12, lr}      @ 保存通用寄存器
    vmov    s0, sp            @ 保存原始 sp

    /* 1. 禁用 counter0 / counter1，防止遗留状态 */
    mov     r3, #( (1 << 0) | (1 << 1) )   @ mask = 0x3
    mcr     p15, 0, r3, c9, c12, 2        @ PMCNTENCLR = 0x3
    isb

    /* 2. 清零 counter0 */
    mov     r2, #0
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 0
    isb
    mcr     p15, 0, r2, c9, c13, 2        @ PMXEVCNTR(0) = 0

    /* 3. 清零 counter1 */
    mov     r2, #1
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 1
    isb
    mov     r2, #0
    mcr     p15, 0, r2, c9, c13, 2        @ PMXEVCNTR(1) = 0

    /* 确保前面的配置都完成 */
    dsb     sy
    isb

    /* 4. 启用 counter0 / counter1 */
    mov     r3, #( (1 << 0) | (1 << 1) )   @ mask = 0x3
    mcr     p15, 0, r3, c9, c12, 1        @ PMCNTENSET = 0x3
    isb
    msr cpsr_f, #0  
    isb

    /* =================================================
     * 5. 被测指令窗口（C 侧会把这一条 nop 覆盖成 hidden insn）
     * ================================================= */
insn_location:
    nop                                   @ 将被覆盖为被测指令

    isb                                   @ 确保该指令完成

    /* 6. 立刻关闭 counter0 / counter1 */
    mcr     p15, 0, r3, c9, c12, 2        @ PMCNTENCLR = 0x3
    isb

    /* 7. 读取 counter0 -> r0 (LD_RETIRED) */
    mov     r2, #0
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 0
    isb
    mrc     p15, 0, r0, c9, c13, 2        @ r0 = PMXEVCNTR(0)

    /* 8. 读取 counter1 -> r1 (ST_RETIRED) */
    mov     r2, #1
    mcr     p15, 0, r2, c9, c12, 5        @ PMSELR = 1
    isb
    mrc     p15, 0, r1, c9, c13, 2        @ r1 = PMXEVCNTR(1)

    /* 9. 返回：
     *    r0 = LD
     *    r1 = ST
     *    在 C 里当成 uint64_t 返回值：
     *      val = ((uint64_t)ST << 32) | LD
     */
    vmov    sp, s0
    pop     {r2-r12, lr}      @ 注意：这里不能把 r0/r1 弹掉，否则把计数覆盖了

    bx      lr

boilerplate_end:

    .size   pmu_template, boilerplate_end - pmu_template
