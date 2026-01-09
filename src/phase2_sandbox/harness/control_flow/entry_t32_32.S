    .syntax unified
    .cpu cortex-a53
    .thumb
    .text
    .align 2
    .global boilerplate_start
    .global insn_location
    .global boilerplate_end
    .global control_flow_template

    .type   control_flow_template, %function
control_flow_template:
boilerplate_start:
    // 1. 保存现场
    push {r0-r12, lr}
    vmov s0, sp

    // 2. 初始化寄存器
    mov r0, #0
    mov r1, #0
    mov lr, #0  // 清空 LR

    // 3. 屏障
    isb
    // T32 Fix: Use mov + msr APSR
    mov     r0, #0
    msr     APSR_nzcvq, r0
    
    isb
    dsb sy

    .align 2
insn_location:
    .space 4        // [Offset 0] 待测指令位 (会被 C 代码覆盖)
    
    // 陷阱指令 (UDF #0 -> SIGILL)
    // 如果待测指令顺序执行到这里，说明没有跳转
    udf #0          // T32 UDF

    // 4. 恢复现场
    vmov sp, s0
    pop {r0-r12, lr}
    bx      lr

boilerplate_end:
    .size   control_flow_template, boilerplate_end - control_flow_template
