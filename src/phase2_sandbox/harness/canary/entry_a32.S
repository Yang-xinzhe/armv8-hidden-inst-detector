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

test_instruction:
boilerplate_start:
    msr cpsr_f, #0  
    dsb

insn_location:
    nop
    mrc p15, 0, r0, c1, c0, 0

    dsb
    bx      lr

    .ltorg

boilerplate_end:
    .size   test_instruction, .-test_instruction
