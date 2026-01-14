#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef TEST_INSTRUCTION
// 默认测试指令 (A32 NOP: 0xe1a00000, A64 NOP: 0xd503201f)
#ifdef __aarch64__
#define TEST_INSTRUCTION 0xd503201f
#else
#define TEST_INSTRUCTION 0xe1a00000
#endif
#endif

#define STR(x) #x
#define XSTR(x) STR(x)

#ifdef __aarch64__

/* ==========================================
 * AArch64 (ARMv8-A 64-bit) Implementation
 * ========================================== */

void test_predefined_instructions(void) {
    uint64_t x0_b, x1_b, x2_b, x3_b, x4_b, x5_b, x6_b, x7_b;
    uint64_t x8_b, x9_b, x10_b, x11_b, x12_b, x13_b, x14_b, x15_b;
    uint64_t x16_b, x17_b, x18_b, x19_b, x20_b, x21_b, x22_b, x23_b;
    uint64_t x24_b, x25_b, x26_b, x27_b, x28_b, x29_b, x30_b;
    uint64_t sp_b, pc_b, nzcv_b;

    uint64_t x0_a, x1_a, x2_a, x3_a, x4_a, x5_a, x6_a, x7_a;
    uint64_t x8_a, x9_a, x10_a, x11_a, x12_a, x13_a, x14_a, x15_a;
    uint64_t x16_a, x17_a, x18_a, x19_a, x20_a, x21_a, x22_a, x23_a;
    uint64_t x24_a, x25_a, x26_a, x27_a, x28_a, x29_a, x30_a;
    uint64_t sp_a, pc_a, nzcv_a;

    printf("Architecture: AArch64\n");
    printf("Testing Instruction: 0x%08x\n", TEST_INSTRUCTION);

    // 为了尽可能少地干扰寄存器，我们不使用 Register 约束 ("=r")
    // 而是使用 Memory 约束 ("=m")，让编译器生成 str 指令。
    // 我们必须保留 x29 (FP) 或 sp 给编译器寻址。
    
    asm volatile (
        // ---------------------------------------------------------
        // 1. 保存 BEFORE 状态
        // ---------------------------------------------------------
        // 保存 x0-x28
        "str x0,  %[x0_b]\n"
        "str x1,  %[x1_b]\n"
        "str x2,  %[x2_b]\n"
        "str x3,  %[x3_b]\n"
        "str x4,  %[x4_b]\n"
        "str x5,  %[x5_b]\n"
        "str x6,  %[x6_b]\n"
        "str x7,  %[x7_b]\n"
        "str x8,  %[x8_b]\n"
        "str x9,  %[x9_b]\n"
        "str x10, %[x10_b]\n"
        "str x11, %[x11_b]\n"
        "str x12, %[x12_b]\n"
        "str x13, %[x13_b]\n"
        "str x14, %[x14_b]\n"
        "str x15, %[x15_b]\n"
        "str x16, %[x16_b]\n"
        "str x17, %[x17_b]\n"
        "str x18, %[x18_b]\n"
        "str x19, %[x19_b]\n"
        "str x20, %[x20_b]\n"
        "str x21, %[x21_b]\n"
        "str x22, %[x22_b]\n"
        "str x23, %[x23_b]\n"
        "str x24, %[x24_b]\n"
        "str x25, %[x25_b]\n"
        "str x26, %[x26_b]\n"
        "str x27, %[x27_b]\n"
        "str x28, %[x28_b]\n"
        "str x29, %[x29_b]\n" // FP
        "str x30, %[x30_b]\n" // LR

        // 保存特殊寄存器
        // 需要一个临时寄存器来读取 SP/NZCV/PC。
        // 我们牺牲 x30 (LR)，因为它已经被保存了，且最后可以恢复
        "mov x30, sp\n"
        "str x30, %[sp_b]\n"
        
        "mrs x30, nzcv\n"
        "str x30, %[nzcv_b]\n"
        
        "adr x30, .\n"
        "str x30, %[pc_b]\n"

        // 恢复 x30 (从内存中读回，保证它是 Clean 的)
        "ldr x30, %[x30_b]\n"

        // ---------------------------------------------------------
        // 2. 执行测试指令
        // ---------------------------------------------------------
        // 保护 SP: AArch64 的 SP 必须对齐，且不能像通用寄存器那样随便操作。
        // 你的模板用 SIMD 寄存器保护 SP，这是一个好主意。
        "mov x30, sp\n"
        "fmov d0, x30\n"
        "ldr x30, %[x30_b]\n" // 再次恢复 x30

        ".word " XSTR(TEST_INSTRUCTION) "\n"

        // ---------------------------------------------------------
        // 3. 恢复 SP (如果指令破坏了它)
        // ---------------------------------------------------------
        // 暂时借用 x30
        // 注意：如果 TEST_INSTRUCTION 修改了 x30，这里会覆盖它。
        // 但为了能继续运行，我们必须保证 SP 有效。
        // 我们先保存此时的 x30 到 after
        "str x30, %[x30_a]\n"
        
        "fmov x30, d0\n"
        "mov sp, x30\n"
        // 现在的 x30 是 SP 的值，不是刚才执行完的值。
        // 所以我们必须在 fmov 之前把 x30 存起来。上面已经存了。
        
        // ---------------------------------------------------------
        // 4. 保存 AFTER 状态
        // ---------------------------------------------------------
        "str x0,  %[x0_a]\n"
        "str x1,  %[x1_a]\n"
        "str x2,  %[x2_a]\n"
        "str x3,  %[x3_a]\n"
        "str x4,  %[x4_a]\n"
        "str x5,  %[x5_a]\n"
        "str x6,  %[x6_a]\n"
        "str x7,  %[x7_a]\n"
        "str x8,  %[x8_a]\n"
        "str x9,  %[x9_a]\n"
        "str x10, %[x10_a]\n"
        "str x11, %[x11_a]\n"
        "str x12, %[x12_a]\n"
        "str x13, %[x13_a]\n"
        "str x14, %[x14_a]\n"
        "str x15, %[x15_a]\n"
        "str x16, %[x16_a]\n"
        "str x17, %[x17_a]\n"
        "str x18, %[x18_a]\n"
        "str x19, %[x19_a]\n"
        "str x20, %[x20_a]\n"
        "str x21, %[x21_a]\n"
        "str x22, %[x22_a]\n"
        "str x23, %[x23_a]\n"
        "str x24, %[x24_a]\n"
        "str x25, %[x25_a]\n"
        "str x26, %[x26_a]\n"
        "str x27, %[x27_a]\n"
        "str x28, %[x28_a]\n"
        "str x29, %[x29_a]\n" 
        // x30 已经在恢复 SP 前保存过了

        // 保存特殊寄存器
        // 再次借用 x30 (此时它已经是脏的或者被用于恢复SP的临时值，无所谓了，因为已经保存了)
        "mov x30, sp\n"
        "str x30, %[sp_a]\n"
        
        "mrs x30, nzcv\n"
        "str x30, %[nzcv_a]\n"
        
        "adr x30, .\n"
        "str x30, %[pc_a]\n"

        // 最后恢复 x30，保证函数能返回 (LR)
        "ldr x30, %[x30_b]\n" // 恢复为初始 LR，确保 ret 不崩

        : [x0_b] "=m"(x0_b), [x1_b] "=m"(x1_b), [x2_b] "=m"(x2_b), [x3_b] "=m"(x3_b),
          [x4_b] "=m"(x4_b), [x5_b] "=m"(x5_b), [x6_b] "=m"(x6_b), [x7_b] "=m"(x7_b),
          [x8_b] "=m"(x8_b), [x9_b] "=m"(x9_b), [x10_b] "=m"(x10_b), [x11_b] "=m"(x11_b),
          [x12_b] "=m"(x12_b), [x13_b] "=m"(x13_b), [x14_b] "=m"(x14_b), [x15_b] "=m"(x15_b),
          [x16_b] "=m"(x16_b), [x17_b] "=m"(x17_b), [x18_b] "=m"(x18_b), [x19_b] "=m"(x19_b),
          [x20_b] "=m"(x20_b), [x21_b] "=m"(x21_b), [x22_b] "=m"(x22_b), [x23_b] "=m"(x23_b),
          [x24_b] "=m"(x24_b), [x25_b] "=m"(x25_b), [x26_b] "=m"(x26_b), [x27_b] "=m"(x27_b),
          [x28_b] "=m"(x28_b), [x29_b] "=m"(x29_b), [x30_b] "=m"(x30_b),
          [sp_b] "=m"(sp_b), [pc_b] "=m"(pc_b), [nzcv_b] "=m"(nzcv_b),

          [x0_a] "=m"(x0_a), [x1_a] "=m"(x1_a), [x2_a] "=m"(x2_a), [x3_a] "=m"(x3_a),
          [x4_a] "=m"(x4_a), [x5_a] "=m"(x5_a), [x6_a] "=m"(x6_a), [x7_a] "=m"(x7_a),
          [x8_a] "=m"(x8_a), [x9_a] "=m"(x9_a), [x10_a] "=m"(x10_a), [x11_a] "=m"(x11_a),
          [x12_a] "=m"(x12_a), [x13_a] "=m"(x13_a), [x14_a] "=m"(x14_a), [x15_a] "=m"(x15_a),
          [x16_a] "=m"(x16_a), [x17_a] "=m"(x17_a), [x18_a] "=m"(x18_a), [x19_a] "=m"(x19_a),
          [x20_a] "=m"(x20_a), [x21_a] "=m"(x21_a), [x22_a] "=m"(x22_a), [x23_a] "=m"(x23_a),
          [x24_a] "=m"(x24_a), [x25_a] "=m"(x25_a), [x26_a] "=m"(x26_a), [x27_a] "=m"(x27_a),
          [x28_a] "=m"(x28_a), [x29_a] "=m"(x29_a), [x30_a] "=m"(x30_a),
          [sp_a] "=m"(sp_a), [pc_a] "=m"(pc_a), [nzcv_a] "=m"(nzcv_a)
        :
        : "d0", "memory" 
        // 关键：不要 clobber x0-x30，因为我们要用它们来传值
        // 但编译器可能会复用 xN 来做地址计算，这会导致冲突。
        // 希望编译器能处理，或者使用 stack frame 指针。
    );

    printf("\n=== General Registers ===\n");
    // 定义一个宏来简化打印
    #define PRINT_REG(N) \
        if (x##N##_b != x##N##_a) printf("x%-2d: 0x%016lx -> 0x%016lx [Changed]\n", N, x##N##_b, x##N##_a);

    PRINT_REG(0); PRINT_REG(1); PRINT_REG(2); PRINT_REG(3);
    PRINT_REG(4); PRINT_REG(5); PRINT_REG(6); PRINT_REG(7);
    PRINT_REG(8); PRINT_REG(9); PRINT_REG(10); PRINT_REG(11);
    PRINT_REG(12); PRINT_REG(13); PRINT_REG(14); PRINT_REG(15);
    PRINT_REG(16); PRINT_REG(17); PRINT_REG(18); PRINT_REG(19);
    PRINT_REG(20); PRINT_REG(21); PRINT_REG(22); PRINT_REG(23);
    PRINT_REG(24); PRINT_REG(25); PRINT_REG(26); PRINT_REG(27);
    PRINT_REG(28); PRINT_REG(29); PRINT_REG(30);

    printf("\n=== Special Registers ===\n");
    if (nzcv_b != nzcv_a) {
        printf("NZCV: 0x%016lx -> 0x%016lx [Changed]\n", nzcv_b, nzcv_a);
        printf("  Flags: ");
        if ((nzcv_b & 0x80000000) != (nzcv_a & 0x80000000)) printf("N ");
        if ((nzcv_b & 0x40000000) != (nzcv_a & 0x40000000)) printf("Z ");
        if ((nzcv_b & 0x20000000) != (nzcv_a & 0x20000000)) printf("C ");
        if ((nzcv_b & 0x10000000) != (nzcv_a & 0x10000000)) printf("V ");
        printf("\n");
    } else {
        printf("NZCV: No Change\n");
    }

    if (sp_b != sp_a) {
        printf("SP:   0x%016lx -> 0x%016lx [Changed]\n", sp_b, sp_a);
    } else {
        printf("SP:   No Change\n");
    }

    printf("PC:   0x%016lx -> 0x%016lx (Offset: %ld)\n", pc_b, pc_a, (long)(pc_a - pc_b));
}

#elif defined(TEST_T32_16BIT) || defined(TEST_T32_32BIT)
/* ==========================================
 * AArch32 (ARMv8-A T32/T16) Implementation
 * ========================================== */
__attribute__((target("thumb"))) 
 void test_predefined_instructions(void) {
    uint32_t r0_before = 0, r1_before = 0, r2_before = 0, r3_before = 0;
    uint32_t r4_before = 0, r5_before = 0, r6_before = 0, r7_before = 0, r8_before = 0, r9_before = 0;
    uint32_t r0_after = 0, r1_after = 0, r2_after = 0, r3_after = 0;
    uint32_t r4_after = 0, r5_after = 0, r6_after = 0, r7_after = 0, r8_after = 0, r9_after = 0;
    uint32_t cpsr_before = 0, cpsr_after = 0;
    uint32_t lr_before = 0, lr_after = 0;
    uint32_t sp_before = 0, sp_after = 0;
    uint32_t pc_before = 0, pc_after = 0;
    
    printf("Architecture: AArch32 (ARM)\n");
    printf("Testing Instruction: 0x%08x\n", TEST_INSTRUCTION);

    __asm__ __volatile__(/* 保存执行前的CPSR和特殊寄存器 */
                "test_instruction:"     
                 "str r9, %[r9_b] \n"
                 "mov r9, #0    \n"
                 "msr APSR_nzcvq, r9   \n"
                 "mrs r9, cpsr \n"
                 "str r9, %[cpsr_b] \n"
                 "str lr, %[lr_b] \n"
                 "str sp, %[sp_b] \n"
                 "ldr r9, %[r9_b] \n"

                 /* 保存执行前的普通寄存器 */
                 "str r0, %[r0_b] \n"
                 "str r1, %[r1_b] \n"
                 "str r2, %[r2_b] \n"
                 "str r3, %[r3_b] \n"
                 "str r4, %[r4_b] \n"
                 "str r5, %[r5_b] \n"
                 "str r6, %[r6_b] \n"
                 "str r7, %[r7_b] \n"
                 "str r8, %[r8_b] \n"

                 "mov r12, pc \n"
                 "str r12, %[pc_b] \n"

                 ".inst.w " XSTR(TEST_INSTRUCTION) "\n"

                 "mov r12, pc \n"
                 "str r12, %[pc_a] \n"

                 "str r0, %[r0_a] \n"
                 "str r1, %[r1_a] \n"
                 "str r2, %[r2_a] \n"
                 "str r3, %[r3_a] \n"
                 "str r4, %[r4_a] \n"
                 "str r5, %[r5_a] \n"
                 "str r6, %[r6_a] \n"
                 "str r7, %[r7_a] \n"
                 "str r8, %[r8_a] \n"
                 "str r9, %[r9_a] \n"

                 "mrs r9, cpsr \n"
                 "str r9, %[cpsr_a] \n"
                 "str lr, %[lr_a] \n"
                 "str sp, %[sp_a] \n"

                 : [r0_b] "=m"(r0_before), [r1_b] "=m"(r1_before),
                   [r2_b] "=m"(r2_before), [r3_b] "=m"(r3_before),
                   [r4_b] "=m"(r4_before), [r5_b] "=m"(r5_before),
                   [r6_b] "=m"(r6_before), [r7_b] "=m"(r7_before),
                   [r8_b] "=m"(r8_before), [r9_b] "=m"(r9_before),
                   [r0_a] "=m"(r0_after), [r1_a] "=m"(r1_after),
                   [r2_a] "=m"(r2_after), [r3_a] "=m"(r3_after),
                   [r4_a] "=m"(r4_after), [r5_a] "=m"(r5_after),
                   [r6_a] "=m"(r6_after), [r7_a] "=m"(r7_after),
                   [r8_a] "=m"(r8_after), [r9_a] "=m"(r9_after),
                   [cpsr_b] "=m"(cpsr_before), [cpsr_a] "=m"(cpsr_after),
                   [lr_b] "=m"(lr_before), [lr_a] "=m"(lr_after),
                   [sp_b] "=m"(sp_before), [sp_a] "=m"(sp_after),
                   [pc_b] "=m"(pc_before), [pc_a] "=m"(pc_after)::"r9", "r12", "memory");

    /* 打印寄存器状态 */
    printf("Initial Registers:\n");
    printf("  r0=0x%08x, r1=0x%08x, r2=0x%08x, r3=0x%08x\n", r0_before, r1_before, r2_before, r3_before);
    printf("  r4=0x%08x, r5=0x%08x, r6=0x%08x, r7=0x%08x\n", r4_before, r5_before, r6_before, r7_before);
    printf("  r8=0x%08x, r9=0x%08x\n", r8_before, r9_before);

    printf("\nAfter Execution:\n");
    printf("  r0=0x%08x %s\n", r0_after, (r0_before != r0_after) ? "[Changed]" : "");
    printf("  r1=0x%08x %s\n", r1_after, (r1_before != r1_after) ? "[Changed]" : "");
    printf("  r2=0x%08x %s\n", r2_after, (r2_before != r2_after) ? "[Changed]" : "");
    printf("  r3=0x%08x %s\n", r3_after, (r3_before != r3_after) ? "[Changed]" : "");
    printf("  r4=0x%08x %s\n", r4_after, (r4_before != r4_after) ? "[Changed]" : "");
    printf("  r5=0x%08x %s\n", r5_after, (r5_before != r5_after) ? "[Changed]" : "");
    printf("  r6=0x%08x %s\n", r6_after, (r6_before != r6_after) ? "[Changed]" : "");
    printf("  r7=0x%08x %s\n", r7_after, (r7_before != r7_after) ? "[Changed]" : "");
    printf("  r8=0x%08x %s\n", r8_after, (r8_before != r8_after) ? "[Changed]" : "");
    printf("  r9=0x%08x %s\n", r9_after, (r9_before != r9_after) ? "[Changed]" : "");

    printf("\n=== Special Registers ===\n");
    printf("CPSR: 0x%08x -> 0x%08x %s\n", cpsr_before, cpsr_after,
           (cpsr_before != cpsr_after) ? "[Changed]" : "");

    if (cpsr_before != cpsr_after)
    {
        printf("  Flags Changed: ");
        if ((cpsr_before & 0x80000000) != (cpsr_after & 0x80000000)) printf("N ");
        if ((cpsr_before & 0x40000000) != (cpsr_after & 0x40000000)) printf("Z ");
        if ((cpsr_before & 0x20000000) != (cpsr_after & 0x20000000)) printf("C ");
        if ((cpsr_before & 0x10000000) != (cpsr_after & 0x10000000)) printf("V ");
        printf("\n");
    }

    printf("LR:   0x%08x -> 0x%08x %s\n", lr_before, lr_after, (lr_before != lr_after) ? "[Changed]" : "");
    printf("SP:   0x%08x -> 0x%08x %s\n", sp_before, sp_after, (sp_before != sp_after) ? "[Changed]" : "");
    printf("PC:   0x%08x -> 0x%08x %s\n", pc_before, pc_after, (pc_after != pc_before + 8) ? "[Non-seq]" : "[Seq]");
}

#else

/* ==========================================
 * AArch32 (ARMv8-A 32-bit) Implementation
 * ========================================== */

void test_predefined_instructions(void) {
    uint32_t r0_before = 0, r1_before = 0, r2_before = 0, r3_before = 0;
    uint32_t r4_before = 0, r5_before = 0, r6_before = 0, r7_before = 0, r8_before = 0, r9_before = 0;
    uint32_t r0_after = 0, r1_after = 0, r2_after = 0, r3_after = 0;
    uint32_t r4_after = 0, r5_after = 0, r6_after = 0, r7_after = 0, r8_after = 0, r9_after = 0;
    uint32_t cpsr_before = 0, cpsr_after = 0;
    uint32_t lr_before = 0, lr_after = 0;
    uint32_t sp_before = 0, sp_after = 0;
    uint32_t pc_before = 0, pc_after = 0;
    
    printf("Architecture: AArch32 (ARM)\n");
    printf("Testing Instruction: 0x%08x\n", TEST_INSTRUCTION);

    __asm__ __volatile__(/* 保存执行前的CPSR和特殊寄存器 */
                "test_instruction:"
                 "str r9, %[r9_b] \n"
                 "msr cpsr_f, #0             \n"
                 "mrs r9, cpsr \n"
                 "str r9, %[cpsr_b] \n"
                 "str lr, %[lr_b] \n"
                 "str sp, %[sp_b] \n"
                 "ldr r9, %[r9_b] \n"

                 /* 保存执行前的普通寄存器 */
                 "str r0, %[r0_b] \n"
                 "str r1, %[r1_b] \n"
                 "str r2, %[r2_b] \n"
                 "str r3, %[r3_b] \n"
                 "str r4, %[r4_b] \n"
                 "str r5, %[r5_b] \n"
                 "str r6, %[r6_b] \n"
                 "str r7, %[r7_b] \n"
                 "str r8, %[r8_b] \n"

                 "str pc, %[pc_b] \n"
                 ".word " XSTR(TEST_INSTRUCTION) "\n"

                 "str pc, %[pc_a] \n"

                 "str r0, %[r0_a] \n"
                 "str r1, %[r1_a] \n"
                 "str r2, %[r2_a] \n"
                 "str r3, %[r3_a] \n"
                 "str r4, %[r4_a] \n"
                 "str r5, %[r5_a] \n"
                 "str r6, %[r6_a] \n"
                 "str r7, %[r7_a] \n"
                 "str r8, %[r8_a] \n"
                 "str r9, %[r9_a] \n"

                 "mrs r9, cpsr \n"
                 "str r9, %[cpsr_a] \n"
                 "str lr, %[lr_a] \n"
                 "str sp, %[sp_a] \n"

                 : [r0_b] "=m"(r0_before), [r1_b] "=m"(r1_before),
                   [r2_b] "=m"(r2_before), [r3_b] "=m"(r3_before),
                   [r4_b] "=m"(r4_before), [r5_b] "=m"(r5_before),
                   [r6_b] "=m"(r6_before), [r7_b] "=m"(r7_before),
                   [r8_b] "=m"(r8_before), [r9_b] "=m"(r9_before),
                   [r0_a] "=m"(r0_after), [r1_a] "=m"(r1_after),
                   [r2_a] "=m"(r2_after), [r3_a] "=m"(r3_after),
                   [r4_a] "=m"(r4_after), [r5_a] "=m"(r5_after),
                   [r6_a] "=m"(r6_after), [r7_a] "=m"(r7_after),
                   [r8_a] "=m"(r8_after), [r9_a] "=m"(r9_after),
                   [cpsr_b] "=m"(cpsr_before), [cpsr_a] "=m"(cpsr_after),
                   [lr_b] "=m"(lr_before), [lr_a] "=m"(lr_after),
                   [sp_b] "=m"(sp_before), [sp_a] "=m"(sp_after),
                   [pc_b] "=m"(pc_before), [pc_a] "=m"(pc_after)::"r9", "memory");

    /* 打印寄存器状态 */
    printf("Initial Registers:\n");
    printf("  r0=0x%08x, r1=0x%08x, r2=0x%08x, r3=0x%08x\n", r0_before, r1_before, r2_before, r3_before);
    printf("  r4=0x%08x, r5=0x%08x, r6=0x%08x, r7=0x%08x\n", r4_before, r5_before, r6_before, r7_before);
    printf("  r8=0x%08x, r9=0x%08x\n", r8_before, r9_before);

    printf("\nAfter Execution:\n");
    printf("  r0=0x%08x %s\n", r0_after, (r0_before != r0_after) ? "[Changed]" : "");
    printf("  r1=0x%08x %s\n", r1_after, (r1_before != r1_after) ? "[Changed]" : "");
    printf("  r2=0x%08x %s\n", r2_after, (r2_before != r2_after) ? "[Changed]" : "");
    printf("  r3=0x%08x %s\n", r3_after, (r3_before != r3_after) ? "[Changed]" : "");
    printf("  r4=0x%08x %s\n", r4_after, (r4_before != r4_after) ? "[Changed]" : "");
    printf("  r5=0x%08x %s\n", r5_after, (r5_before != r5_after) ? "[Changed]" : "");
    printf("  r6=0x%08x %s\n", r6_after, (r6_before != r6_after) ? "[Changed]" : "");
    printf("  r7=0x%08x %s\n", r7_after, (r7_before != r7_after) ? "[Changed]" : "");
    printf("  r8=0x%08x %s\n", r8_after, (r8_before != r8_after) ? "[Changed]" : "");
    printf("  r9=0x%08x %s\n", r9_after, (r9_before != r9_after) ? "[Changed]" : "");

    printf("\n=== Special Registers ===\n");
    printf("CPSR: 0x%08x -> 0x%08x %s\n", cpsr_before, cpsr_after,
           (cpsr_before != cpsr_after) ? "[Changed]" : "");

    if (cpsr_before != cpsr_after)
    {
        printf("  Flags Changed: ");
        if ((cpsr_before & 0x80000000) != (cpsr_after & 0x80000000)) printf("N ");
        if ((cpsr_before & 0x40000000) != (cpsr_after & 0x40000000)) printf("Z ");
        if ((cpsr_before & 0x20000000) != (cpsr_after & 0x20000000)) printf("C ");
        if ((cpsr_before & 0x10000000) != (cpsr_after & 0x10000000)) printf("V ");
        printf("\n");
    }

    printf("LR:   0x%08x -> 0x%08x %s\n", lr_before, lr_after, (lr_before != lr_after) ? "[Changed]" : "");
    printf("SP:   0x%08x -> 0x%08x %s\n", sp_before, sp_after, (sp_before != sp_after) ? "[Changed]" : "");
    printf("PC:   0x%08x -> 0x%08x %s\n", pc_before, pc_after, (pc_after != pc_before + 8) ? "[Non-seq]" : "[Seq]");
}
#endif

int main() {
    test_predefined_instructions();
    return 0;
}