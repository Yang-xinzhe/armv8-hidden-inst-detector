    .syntax unified
    .cpu cortex-a53
    .thumb
    .text
    .align 2

    .global test_instruction
    .global boilerplate_start
    .global insn_location
    .global boilerplate_end

    .type   test_instruction, %function

test_instruction:
boilerplate_start:
    mov r3, #0
    msr APSR_nzcvq, r3
    dsb

    .align 2
insn_location:
    .space 4

    @ Canary Instruction: Read SCTLR (Privileged)
    @ If previous instruction escalated privilege, this should succeed.
    @ If not, this should trap (SIGILL).
    mrc p15, 0, r0, c1, c0, 0

    dsb
    bx      lr

    .ltorg
    .align 2 
boilerplate_end:
    .size   test_instruction, .-test_instruction
