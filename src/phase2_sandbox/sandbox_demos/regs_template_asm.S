    .syntax unified
    .cpu cortex-a53
    .arm
    .text
    .align 2

    .global test_instruction
    .global boilerplate_start
    .global insn_location
    .global boilerplate_end

    .type   test_instruction, %function

    .extern reg_state_base_slot     @ 在 C 里定义的全局指针

@ void test_instruction(void);
test_instruction:
boilerplate_start:
    @ --------- 初始化通用寄存器 ----------
    mov     r0, #0
    ldr     r1, =0x11111111
    ldr     r2, =0x22222222
    ldr     r3, =0x33333333
    ldr     r4, =0x44444444
    ldr     r5, =0x55555555
    ldr     r6, =0x66666666
    ldr     r7, =0x77777777
    ldr     r8, =0x88888888
    ldr     r9, =0x99999999
    ldr     r10, =0xaaaaaaaa
    ldr     r11, =0xbbbbbbbb
    ldr     r12, =0xcccccccc

    @ 保存初始化后的 GPR 状态（before）
    push    {r0-r12, lr}

    @ r0 = reg_state_base_slot (RegisterStates * 的基址)
    ldr     r0, =reg_state_base_slot
    ldr     r0, [r0]             @ r0 = reg_state_base_slot

    @ ---- 存 r0 / r1 ----
    ldr     r1, [sp, #0]
    str     r1, [r0, #0]

    ldr     r1, [sp, #4]
    str     r1, [r0, #4]

    @ ---- 存 r2-r11 (10 个寄存器，总共 40 字节) ----
    add     r1, sp, #8           @ r1 -> 栈上的 r2
    add     r2, r0, #8           @ r2 -> 目标中的 r2
    ldmia   r1!, {r3-r12}        @ 从栈读 r2-r11 (共 10 word)
    stmia   r2!, {r3-r12}        @ 写入目标 r2-r11

    @ ---- 单独处理 r12 ----
    ldr     r1, [sp, #48]        @ 栈上的 r12
    str     r1, [r0, #48]        @ states[0].r12

    @ ---- CPSR ----
    @ Offset = 13 regs * 4 bytes = 52
    msr cpsr_f, #0  
    mrs     r1, cpsr
    
    str     r1, [r0, #52]        @ states[0].cpsr

    @ 注意：不再保存 sp, lr, pc

    @ 恢复寄存器，准备执行测试指令
    pop     {r0-r12, lr}

    dsb

insn_location:
    nop                         @ 这里会被替换成待测指令

    dsb

    @ --------- 第二段：执行后寄存器状态 ----------
    push    {r0-r12, lr}

    @ r0 = reg_state_base_slot + 56 => 第二个 RegisterStates
    ldr     r0, =reg_state_base_slot
    ldr     r0, [r0]
    add     r0, r0, #56          @ sizeof(RegisterStates) = 56 字节

    @ ---- 存 r0 / r1 ----
    ldr     r1, [sp, #0]
    str     r1, [r0, #0]

    ldr     r1, [sp, #4]
    str     r1, [r0, #4]

    @ ---- 存 r2-r11 ----
    add     r1, sp, #8
    add     r2, r0, #8
    ldmia   r1!, {r3-r12}
    stmia   r2!, {r3-r12}

    @ ---- r12 ----
    ldr     r1, [sp, #48]
    str     r1, [r0, #48]

    @ ---- cpsr ----
    mrs     r1, cpsr
    str     r1, [r0, #52]        @ states[1].cpsr

    @ 恢复并返回
    pop     {r0-r12, lr}
    bx      lr

    .ltorg

boilerplate_end:
    .size   test_instruction, .-test_instruction
