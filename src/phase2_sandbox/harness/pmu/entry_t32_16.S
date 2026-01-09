    .syntax unified
    .cpu cortex-a53
    .thumb
    .text
    .align 2

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

    mrc     p15, 0, r3, c9, c12, 0       @ 读取 PMCR
    orr     r3, r3, #1                   @ 置位 bit 0 (Enable)
    mcr     p15, 0, r3, c9, c12, 0       @ 写回 PMCR
    isb                                  @ 确保指令完成

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
    
    @ T32 Fix: Use mov + msr APSR
    mov     r2, #0
    msr     APSR_nzcvq, r2
    
    isb
    dsb sy
    
    .align 2
insn_location:
    .space 4                              @ 预留 4 字节

    dsb sy
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

    vmov    sp, s0
    pop     {r2-r12, lr}      @ 恢复 r2-r12 (保留 r0/r1 返回值)

    bx      lr

    .align 2
boilerplate_end:
    .size   pmu_template, boilerplate_end - pmu_template
