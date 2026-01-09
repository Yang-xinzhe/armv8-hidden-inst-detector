    .syntax unified
    .cpu cortex-a53
    .fpu neon
    .arm
    .text
    .align 4

    .global test_simd_instruction
    .global boilerplate_start
    .global insn_location
    .global boilerplate_end
    .extern simd_state_base_slot

    .type   test_simd_instruction, %function

test_simd_instruction:
boilerplate_start:
    push    {r0-r4, lr}
    vpush   {q4-q7}              @ 保存被调用者保存寄存器

    @ --------- 初始化 SIMD 寄存器 (使用 GPR 初始化) ----------
    @ 将 Q0-Q15 初始化为 0x00-0x0F 的填充模式 (Q0=00, Q1=11, Q2=22... Q15=FF)
    
    mov     r0, #0x00
    vdup.8  q0, r0
    mov     r0, #0x11
    vdup.8  q1, r0
    mov     r0, #0x22
    vdup.8  q2, r0
    mov     r0, #0x33
    vdup.8  q3, r0
    mov     r0, #0x44
    vdup.8  q4, r0
    mov     r0, #0x55
    vdup.8  q5, r0
    mov     r0, #0x66
    vdup.8  q6, r0
    mov     r0, #0x77
    vdup.8  q7, r0
    mov     r0, #0x88
    vdup.8  q8, r0
    mov     r0, #0x99
    vdup.8  q9, r0
    mov     r0, #0xAA
    vdup.8  q10, r0
    mov     r0, #0xBB
    vdup.8  q11, r0
    mov     r0, #0xCC
    vdup.8  q12, r0
    mov     r0, #0xDD
    vdup.8  q13, r0
    mov     r0, #0xEE
    vdup.8  q14, r0
    mov     r0, #0xFF
    vdup.8  q15, r0

    @ --------- 第一段：保存执行前状态 (Before) ----------
    ldr     r0, =simd_state_base_slot
    ldr     r0, [r0]             @ r0 指向 states[0]

    @ 保存 Q0-Q15
    @ r0 会自增
    vst1.64 {q0-q1}, [r0]!
    vst1.64 {q2-q3}, [r0]!
    vst1.64 {q4-q5}, [r0]!
    vst1.64 {q6-q7}, [r0]!
    vst1.64 {q8-q9}, [r0]!
    vst1.64 {q10-q11}, [r0]!
    vst1.64 {q12-q13}, [r0]!
    vst1.64 {q14-q15}, [r0]!

    @ 保存 FPSCR
    vmrs    r1, fpscr
    str     r1, [r0]

    @ --------- 待测指令位置 ----------
    dsb
    .ltorg  @ 确保 Literal Pool 在这里，在 insn_location 之前
insn_location:
    nop
    dsb

    @ --------- 第二段：保存执行后状态 (After) ----------
    ldr     r0, =simd_state_base_slot
    ldr     r0, [r0]
    add     r0, r0, #272         @ 跳转到 states[1] (256 + 4 + 12 padding)

    vst1.64 {q0-q1}, [r0]!
    vst1.64 {q2-q3}, [r0]!
    vst1.64 {q4-q5}, [r0]!
    vst1.64 {q6-q7}, [r0]!
    vst1.64 {q8-q9}, [r0]!
    vst1.64 {q10-q11}, [r0]!
    vst1.64 {q12-q13}, [r0]!
    vst1.64 {q14-q15}, [r0]!

    vmrs    r1, fpscr
    str     r1, [r0]

    @ --------- 恢复环境并返回 ----------
    vpop    {q4-q7}
    pop     {r0-r4, pc}
    .ltorg
boilerplate_end:
    .size   test_simd_instruction, .-test_simd_instruction

    @ --- Helper to generate hex codes ---
generate_hex:
    vorr q0, q1, q1
    vorr q1, q2, q2
    vorr q2, q3, q3
    vorr q3, q4, q4
    vorr q4, q5, q5
    vorr q5, q6, q6
    vorr q6, q7, q7
    vorr q7, q8, q8
    vorr q8, q9, q9
    vorr q9, q10, q10
    vorr q10, q11, q11
    vorr q11, q12, q12
    vorr q12, q13, q13
    vorr q13, q14, q14
    vorr q14, q15, q15
    vorr q15, q0, q0
