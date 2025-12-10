#include "core.h"
#include "sandbox.h"
#include "register_states.h"

RegisterStates *reg_state_base_slot = NULL; 

int init_reg_state_slot(void)
{
    // 存 before/after 两份寄存器
    reg_state_base_slot = malloc(2 * sizeof(RegisterStates));
    if (!reg_state_base_slot) {
        perror("malloc reg_state_base_slot");
        return -1;
    }
    memset(reg_state_base_slot, 0, 2 * sizeof(RegisterStates));
    return 0;
}

void print_regs_diff(const RegisterStates *before, const RegisterStates *after)
{
    printf("=== Register Differences ===\n");

    // 宏：如果寄存器变化了就打印
    #define PRINT_DIFF(reg) \
        if (before->reg != after->reg) { \
            printf("%-4s: %08x -> %08x\n", #reg, before->reg, after->reg); \
        }

    PRINT_DIFF(r0);  PRINT_DIFF(r1);  PRINT_DIFF(r2);  PRINT_DIFF(r3);
    PRINT_DIFF(r4);  PRINT_DIFF(r5);  PRINT_DIFF(r6);  PRINT_DIFF(r7);
    PRINT_DIFF(r8);  PRINT_DIFF(r9);  PRINT_DIFF(r10); PRINT_DIFF(r11);
    PRINT_DIFF(r12); PRINT_DIFF(cpsr);

    #undef PRINT_DIFF
}


int main() {
    uint32_t hidden_instruction = 0xE0800001;
    uint32_t test_alu_insns[] = {
        // ============= 基本算术运算 =============
        // ADD - 各种寄存器组合
        0xE0823004,   // ADD r3, r2, r4
        0xE0834005,   // ADD r4, r3, r5
        0xE0845006,   // ADD r5, r4, r6
        0xE0856007,   // ADD r6, r5, r7
        0xE0867008,   // ADD r7, r6, r8
        0xE0878009,   // ADD r8, r7, r9
        0xE088900A,   // ADD r9, r8, r10
        0xE089A002,   // ADD r10, r9, r2
        
        // SUB - 各种寄存器组合
        0xE0432005,   // SUB r3, r3, r5
        0xE0543006,   // SUB r4, r4, r6
        0xE0654007,   // SUB r5, r5, r7
        0xE0765008,   // SUB r6, r6, r8
        0xE0876009,   // SUB r7, r7, r9
        0xE098700A,   // SUB r8, r8, r10
        0xE0A98002,   // SUB r9, r9, r2
        0xE0BA9003,   // SUB r10, r10, r3
        
        // ADC/SBC - 带进位/借位
        0xE0A23004,   // ADC r3, r2, r4
        0xE0C34005,   // SBC r4, r3, r5
        0xE0A45006,   // ADC r5, r4, r6
        0xE0C56007,   // SBC r6, r5, r7
        0xE0A67008,   // ADC r7, r6, r8
        0xE0C78009,   // SBC r8, r7, r9
        0xE0A8900A,   // ADC r9, r8, r10
        0xE0C9A002,   // SBC r10, r9, r2
        
        // RSB/RSC - 反向运算
        0xE0623004,   // RSB r3, r2, r4
        0xE0634005,   // RSB r4, r3, r5
        0xE0645006,   // RSB r5, r4, r6
        0xE0E56007,   // RSC r6, r5, r7
        0xE0657008,   // RSB r7, r6, r8
        0xE0E68009,   // RSC r8, r7, r9
        0xE067900A,   // RSB r9, r8, r10
        0xE0E8A002,   // RSC r10, r9, r2
        
        // ============= 逻辑运算 =============
        // AND - 与运算
        0xE0023004,   // AND r3, r2, r4
        0xE0034005,   // AND r4, r3, r5
        0xE0045006,   // AND r5, r4, r6
        0xE0056007,   // AND r6, r5, r7
        0xE0067008,   // AND r7, r6, r8
        0xE0078009,   // AND r8, r7, r9
        0xE008900A,   // AND r9, r8, r10
        0xE009A002,   // AND r10, r9, r2
        
        // ORR - 或运算
        0xE1823004,   // ORR r3, r2, r4
        0xE1834005,   // ORR r4, r3, r5
        0xE1845006,   // ORR r5, r4, r6
        0xE1856007,   // ORR r6, r5, r7
        0xE1867008,   // ORR r7, r6, r8
        0xE1878009,   // ORR r8, r7, r9
        0xE188900A,   // ORR r9, r8, r10
        0xE189A002,   // ORR r10, r9, r2
        
        // EOR - 异或运算
        0xE0223004,   // EOR r3, r2, r4
        0xE0234005,   // EOR r4, r3, r5
        0xE0245006,   // EOR r5, r4, r6
        0xE0256007,   // EOR r6, r5, r7
        0xE0267008,   // EOR r7, r6, r8
        0xE0278009,   // EOR r8, r7, r9
        0xE028900A,   // EOR r9, r8, r10
        0xE029A002,   // EOR r10, r9, r2
        
        // BIC - 位清除
        0xE1C23004,   // BIC r3, r2, r4
        0xE1C34005,   // BIC r4, r3, r5
        0xE1C45006,   // BIC r5, r4, r6
        0xE1C56007,   // BIC r6, r5, r7
        0xE1C67008,   // BIC r7, r6, r8
        0xE1C78009,   // BIC r8, r7, r9
        0xE1C8900A,   // BIC r9, r8, r10
        0xE1C9A002,   // BIC r10, r9, r2
        
        // ============= 比较和移动 =============
        // MOV - 移动
        0xE1A03004,   // MOV r3, r4
        0xE1A04005,   // MOV r4, r5
        0xE1A05006,   // MOV r5, r6
        0xE1A06007,   // MOV r6, r7
        0xE1A07008,   // MOV r7, r8
        0xE1A08009,   // MOV r8, r9
        0xE1A0900A,   // MOV r9, r10
        0xE1A0A002,   // MOV r10, r2
        
        // MVN - 取反移动
        0xE1E03004,   // MVN r3, r4
        0xE1E04005,   // MVN r4, r5
        0xE1E05006,   // MVN r5, r6
        0xE1E06007,   // MVN r6, r7
        0xE1E07008,   // MVN r7, r8
        0xE1E08009,   // MVN r8, r9
        0xE1E0900A,   // MVN r9, r10
        0xE1E0A002,   // MVN r10, r2
        
        // ============= 乘法运算 =============
        // MUL - 乘法
        0xE0020394,   // MUL r2, r4, r3
        0xE0030495,   // MUL r3, r5, r4
        0xE0040596,   // MUL r4, r6, r5
        0xE0050697,   // MUL r5, r7, r6
        0xE0060798,   // MUL r6, r8, r7
        0xE0070899,   // MUL r7, r9, r8
        0xE00809AA,   // MUL r8, r10, r9
        0xE0090A23,   // MUL r9, r3, r10
        
        // MLA - 乘加
        0xE0234395,   // MLA r3, r5, r3, r4
        0xE0345496,   // MLA r4, r6, r4, r5
        0xE0456597,   // MLA r5, r7, r5, r6
        0xE0567698,   // MLA r6, r8, r6, r7
        0xE0678799,   // MLA r7, r9, r7, r8
        0xE07898AA,   // MLA r8, r10, r8, r9
        0xE089A923,   // MLA r9, r3, r9, r10
        0xE09A3A24,   // MLA r10, r4, r10, r3
        
        // MLS - 乘减
        0xE0634395,   // MLS r3, r5, r3, r4
        0xE0745496,   // MLS r4, r6, r4, r5
        0xE0856597,   // MLS r5, r7, r5, r6
        0xE0967698,   // MLS r6, r8, r6, r7
        0xE0A78799,   // MLS r7, r9, r7, r8
        0xE0B898AA,   // MLS r8, r10, r8, r9
        0xE0C9A923,   // MLS r9, r3, r9, r10
        0xE0DA3A24,   // MLS r10, r4, r10, r3
        
        // ============= 长乘法运算 =============
        // UMULL - 无符号长乘法
        0xE0821493,   // UMULL r1, r2, r3, r4
        0xE0932594,   // UMULL r2, r3, r4, r5
        0xE0A43695,   // UMULL r3, r4, r5, r6
        0xE0B54796,   // UMULL r4, r5, r6, r7
        0xE0C65897,   // UMULL r5, r6, r7, r8
        0xE0D76998,   // UMULL r6, r7, r8, r9
        0xE0E87A99,   // UMULL r7, r8, r9, r10
        0xE0F98B2A,   // UMULL r8, r9, r10, r2
        
        // SMULL - 有符号长乘法
        0xE0C21493,   // SMULL r1, r2, r3, r4
        0xE0D32594,   // SMULL r2, r3, r4, r5
        0xE0E43695,   // SMULL r3, r4, r5, r6
        0xE0F54796,   // SMULL r4, r5, r6, r7
        0xE0C65897,   // SMULL r5, r6, r7, r8
        0xE0D76998,   // SMULL r6, r7, r8, r9
        0xE0E87A99,   // SMULL r7, r8, r9, r10
        0xE0F98B2A,   // SMULL r8, r9, r10, r2
        
        // ============= 带移位器的运算 =============
        // ADD with shift
        0xE0823084,   // ADD r3, r2, r4, LSL #1
        0xE0834105,   // ADD r4, r3, r5, LSL #2
        0xE0845206,   // ADD r5, r4, r6, LSL #4
        0xE0856307,   // ADD r6, r5, r7, LSL #6
        0xE0867408,   // ADD r7, r6, r8, LSL #8
        0xE0878509,   // ADD r8, r7, r9, LSL #10
        0xE088960A,   // ADD r9, r8, r10, LSL #12
        0xE089A702,   // ADD r10, r9, r2, LSL #14
        
        // SUB with shift
        0xE0423284,   // SUB r3, r2, r4, LSL #5
        0xE0534325,   // SUB r4, r3, r5, LSR #6
        0xE0645426,   // SUB r5, r4, r6, ASR #8
        0xE0756527,   // SUB r6, r5, r7, ROR #10
        0xE0867628,   // SUB r7, r6, r8, LSL #12
        0xE0978729,   // SUB r8, r7, r9, LSR #14
        0xE0A8982A,   // SUB r9, r8, r10, ASR #16
        0xE0B9A922,   // SUB r10, r9, r2, ROR #18
        
        // ORR with shift
        0xE1823084,   // ORR r3, r2, r4, LSL #1
        0xE1834105,   // ORR r4, r3, r5, LSL #2
        0xE1845206,   // ORR r5, r4, r6, LSL #4
        0xE1856307,   // ORR r6, r5, r7, LSL #6
        0xE1867408,   // ORR r7, r6, r8, LSL #8
        0xE1878509,   // ORR r8, r7, r9, LSL #10
        0xE188960A,   // ORR r9, r8, r10, LSL #12
        0xE189A702,   // ORR r10, r9, r2, LSL #14
        
        // ============= 除法运算 =============
        // SDIV - 有符号除法
        0xE713F234,   // SDIV r3, r4, r2
        0xE734F245,   // SDIV r4, r5, r3
        0xE745F256,   // SDIV r5, r6, r4
        0xE756F267,   // SDIV r6, r7, r5
        0xE767F278,   // SDIV r7, r8, r6
        0xE778F289,   // SDIV r8, r9, r7
        0xE789F29A,   // SDIV r9, r10, r8
        0xE79AF2A3,   // SDIV r10, r3, r9
        
        // UDIV - 无符号除法
        0xE713F334,   // UDIV r3, r4, r2
        0xE734F345,   // UDIV r4, r5, r3
        0xE745F356,   // UDIV r5, r6, r4
        0xE756F367,   // UDIV r6, r7, r5
        0xE767F378,   // UDIV r7, r8, r6
        0xE778F389,   // UDIV r8, r9, r7
        0xE789F39A,   // UDIV r9, r10, r8
        0xE79AF3A3,   // UDIV r10, r3, r9
        
        // ============= 扩展运算 =============
        // SXTB - 字节符号扩展
        0xE6AF2074,   // SXTB r2, r4
        0xE6AF3075,   // SXTB r3, r5
        0xE6AF4076,   // SXTB r4, r6
        0xE6AF5077,   // SXTB r5, r7
        0xE6AF6078,   // SXTB r6, r8
        0xE6AF7079,   // SXTB r7, r9
        0xE6AF807A,   // SXTB r8, r10
        0xE6AF9073,   // SXTB r9, r3
        
        // UXTB - 字节零扩展
        0xE6EF2074,   // UXTB r2, r4
        0xE6EF3075,   // UXTB r3, r5
        0xE6EF4076,   // UXTB r4, r6
        0xE6EF5077,   // UXTB r5, r7
        0xE6EF6078,   // UXTB r6, r8
        0xE6EF7079,   // UXTB r7, r9
        0xE6EF807A,   // UXTB r8, r10
        0xE6EF9073,   // UXTB r9, r3
        
        // SXTH - 半字符号扩展
        0xE6BF2074,   // SXTH r2, r4
        0xE6BF3075,   // SXTH r3, r5
        0xE6BF4076,   // SXTH r4, r6
        0xE6BF5077,   // SXTH r5, r7
        0xE6BF6078,   // SXTH r6, r8
        0xE6BF7079,   // SXTH r7, r9
        0xE6BF807A,   // SXTH r8, r10
        0xE6BF9073,   // SXTH r9, r3
        
        // UXTH - 半字零扩展
        0xE6FF2074,   // UXTH r2, r4
        0xE6FF3075,   // UXTH r3, r5
        0xE6FF4076,   // UXTH r4, r6
        0xE6FF5077,   // UXTH r5, r7
        0xE6FF6078,   // UXTH r6, r8
        0xE6FF7079,   // UXTH r7, r9
        0xE6FF807A,   // UXTH r8, r10
        0xE6FF9073,   // UXTH r9, r3
        
        // ============= 半字乘加运算 =============
        // SMLABB
        0xE1020493,   // SMLABB r2, r3, r4, r4
        0xE1031594,   // SMLABB r3, r4, r5, r5
        0xE1042695,   // SMLABB r4, r5, r6, r6
        0xE1053796,   // SMLABB r5, r6, r7, r7
        0xE1064897,   // SMLABB r6, r7, r8, r8
        0xE1075998,   // SMLABB r7, r8, r9, r9
        0xE1086A99,   // SMLABB r8, r9, r10, r10
        0xE1097B2A,   // SMLABB r9, r10, r2, r2
        
        // SMLABT
        0xE1021493,   // SMLABT r2, r3, r4, r4
        0xE1032594,   // SMLABT r3, r4, r5, r5
        0xE1043695,   // SMLABT r4, r5, r6, r6
        0xE1054796,   // SMLABT r5, r6, r7, r7
        0xE1065897,   // SMLABT r6, r7, r8, r8
        0xE1076998,   // SMLABT r7, r8, r9, r9
        0xE1087A99,   // SMLABT r8, r9, r10, r10
        0xE1098B2A,   // SMLABT r9, r10, r2, r2
    };

    unsigned int len = sizeof(test_alu_insns)/sizeof(test_alu_insns[0]);


    if (init_reg_state_slot() != 0) return 1;

    init_signal_handler(signal_handler, SIGILL,    SA_NONE);
    init_signal_handler(signal_handler, SIGSEGV,   SA_NONE);
    init_signal_handler(signal_handler, SIGTRAP,   SA_NONE);
    init_signal_handler(signal_handler, SIGBUS,    SA_NONE);

    init_signal_handler(signal_handler, SIGRTMIN,  SA_NODEFER);
    init_signal_handler(signal_handler, SIGVTALRM, SA_NODEFER);


    if (init_watchdog_timer() != 0) {
        fprintf(stderr, "Failed to initialize watchdog timer\n");
        return 1;
    }

    if (init_insn_page() != 0) {
        perror("init_insn_page");
        return 1;
    }

    uint8_t insn_bytes[4];
    for(unsigned int i = 0; i < len; i++) {
        size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), test_alu_insns[i]);
        execute_insn_page_reg(insn_bytes, buf_length, NULL);
        printf("Instruction: 0x%x\n",test_alu_insns[i]);
        print_regs_diff(&reg_state_base_slot[0], &reg_state_base_slot[1]);
        printf("\n");
    }
    



    return 0;
}