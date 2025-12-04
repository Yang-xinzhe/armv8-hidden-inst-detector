.syntax unified
    .cpu cortex-a53
    .arm
    .text
    .align 2

    .global test_instruction
    .global boilerplate_start
    .global insn_location
    .global boilerplate_end
    @ 移除 reg_state_base_slot 的导出

    .type test_instruction, %function

test_instruction:
boilerplate_start:
    @ ==============================================
    @ 输入: r0 包含了 RegisterStates* 数组的基地址
    @ ==============================================

    @ 1. 保存现场，最关键的是保存传入的 r0 (states 指针)
    @    同时保存 lr 以便返回
    push    {r0, lr}

    @ 2. 寄存器初始化 (这会覆盖当前的 r0, 但我们已经在栈里备份了)
    adr     r3, reg_init_values
    ldmia   r3, {r0-r12}

    @ ==============================================
    @ 保存执行前状态 (RegisterStates[0])
    @ ==============================================
    
    @ A. 先把当前所有的测试值压栈保存，腾出寄存器来处理指针
    push    {r0-r12}

    @ B. 从最初的栈位置取回 states 指针
    @    当前的栈结构: [r0-r12 (13*4 bytes)] [original_r0 (states)] [original_lr]
    @    偏移量 = 13 * 4 = 52
    ldr     r0, [sp, #52]       @ r0 现在指向 states[0]

    @ C. 保存通用寄存器 r0-r12 到结构体
    @    此时真实寄存器里的值是混乱的(r0是指针)，我们需要从栈顶把刚才备份的值存进去
    
    @    保存 r0 (测试值)
    ldr     lr, [sp, #0]        @ 从栈顶读回 r0 的测试值
    str     lr, [r0, #0]        @ 存入 states[0].r0

    @    保存 r1-r12 (可以直接用寄存器里的值，因为它们还没被破坏，除了r0)
    @    为了代码规整，我们用 stm 存 r1-r12
    add     r0, r0, #4          @ 跳过 struct.r0
    stm     r0, {r1-r12}        
    sub     r0, r0, #4          @ 恢复 r0 指向结构体头部

    @ D. 保存特殊寄存器
    ldr     r1, [sp, #56]       @ 取回 original_lr (offset: 13*4 + 4)
    str     r1, [r0, #56]       @ states[0].lr

    mrs     r1, cpsr
    str     r1, [r0, #64]       @ states[0].cpsr

    mov     r1, sp
    add     r1, r1, #52         @ 计算 push {r0-r12} 之前的 SP
    add     r1, r1, #8          @ 计算 push {r0, lr} 之前的 SP (原始调用者SP)
    str     r1, [r0, #52]       @ states[0].sp
    
    mov     r1, pc              @ PC
    str     r1, [r0, #60]

    @ E. 恢复现场，准备执行测试指令
    @    我们需要恢复 r0 (测试值)，r1-r12 保持不变
    pop     {r0-r12}            @ 恢复所有测试值，现在 r0 又是 0x00000000 了

    dsb

insn_location:
    nop
    
    dsb

    @ ==============================================
    @ 保存执行后状态 (RegisterStates[1])
    @ ==============================================
    
    push    {r0-r12}            @ 再次备份

    ldr     r0, [sp, #52]       @ 取回 states 指针
    add     r0, r0, #68         @ 偏移到 states[1] (sizeof RegisterStates)

    @ --- 保存逻辑同上 ---
    ldr     lr, [sp, #0]        @ r0 测试值
    str     lr, [r0, #0]

    add     r0, r0, #4
    stm     r0, {r1-r12}
    sub     r0, r0, #4

    ldr     r1, [sp, #56]       @ original_lr
    str     r1, [r0, #56]

    mrs     r1, cpsr
    str     r1, [r0, #64]

    mov     r1, sp
    add     r1, r1, #60         @ 原始 SP 计算
    str     r1, [r0, #52]

    mov     r1, pc
    str     r1, [r0, #60]

    @ F. 返回 Main
    pop     {r0-r12}            @ 弹出测试值
    pop     {r0, pc}            @ 弹出 original_r0 (忽略) 和 lr (直接跳回)

    .align 2
reg_init_values:
    .word   0x00000000, 0x11111111, 0x22222222, 0x33333333
    .word   0x44444444, 0x55555555, 0x66666666, 0x77777777
    .word   0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB
    .word   0xCCCCCCCC

    @ 移除 reg_state_base_slot

boilerplate_end: