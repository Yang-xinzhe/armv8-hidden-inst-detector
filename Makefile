CC				:=	arm-linux-gnueabihf-gcc

NUM_CORES		?=	4

CFLAGS			:=	-std=c11 -Wall -Wextra  -O0 \
           			-marm -march=armv8-a -mfpu=vfpv4 -fomit-frame-pointer -mfloat-abi=hard \
           			-Iinc \

BUILD_DIR   	:= 	build
DISPATCHER		:=	$(BUILD_DIR)/dispatcher
WORKER			:=	$(BUILD_DIR)/worker
MACRO_VALID		:=	$(BUILD_DIR)/macro_valid
REGS_DEMO		:=	$(BUILD_DIR)/regs_demo
PMU_DEMO		:=	$(BUILD_DIR)/pmu_demo
FUZZ_ARITHMETIC 	:=	$(BUILD_DIR)/fuzzer_arithmetic
FUZZ_MEMACCESS		:=	$(BUILD_DIR)/fuzzer_memaccess
CONTROL_FLOW_DEMO := $(BUILD_DIR)/control_flow_demo
FUZZ_CONTROL_FLOW 	:= 	$(BUILD_DIR)/fuzzer_control_flow
SIMD_DEMO		:=	$(BUILD_DIR)/simd_demo
FUZZ_SIMD			:=	$(BUILD_DIR)/fuzzer_simd

COMMON_SRC		:= src/core/cpu_affinity.c 									\
				   src/core/bitmap.c											\
				   src/core/config.c

SANDBOX_SRC 	:= src/core/sandbox.c

REGS_TEMPLATE_SRC := src/phase2_sandbox/sandbox_demos/regs_template_asm.S

DISPATCHER_SRCS	:= src/phase1_screening/dispatcher_screen.c 				\
				   $(COMMON_SRC)

WORKER_SRCS		:= src/phase1_screening/worker_screen.c						\
				   $(SANDBOX_SRC)											\
				   $(COMMON_SRC)

MACRO_SRCS		:= src/phase2_sandbox/macro_valid.c

REGS_DSRCS		:= src/phase2_sandbox/sandbox_demos/regs_template.c 		\
                   $(SANDBOX_SRC) 											\
                   $(REGS_TEMPLATE_SRC)
REGS_DOBJS 		:= $(REGS_DSRCS:.c=.o)
REGS_DOBJS 		:= $(REGS_DOBJS:.S=.o)

PMU_DSRCS		:= src/phase2_sandbox/sandbox_demos/pmu_template.c				\
					$(SANDBOX_SRC)\
				   src/core/pmu_counter.c									\
				   src/phase2_sandbox/sandbox_demos/pmu_template_asm.S		
				   
FUZZ_ARITHMETIC_SRCS := src/phase2_sandbox/fuzzer_arithmetic.c \
                   $(SANDBOX_SRC) \
                   $(REGS_TEMPLATE_SRC) \
                   $(COMMON_SRC)
FUZZ_ARITHMETIC_OBJS := $(FUZZ_ARITHMETIC_SRCS:.c=.o)
FUZZ_ARITHMETIC_OBJS := $(FUZZ_ARITHMETIC_OBJS:.S=.o)
				   
FUZZ_MEMACCESS_SRCS	:=	src/phase2_sandbox/fuzzer_memaccess.c \
				   src/core/pmu_counter.c \
				   $(SANDBOX_SRC) \
				   $(COMMON_SRC)	\
				   src/phase2_sandbox/sandbox_demos/pmu_template_asm.S


CONTROL_FLOW_SRCS := src/phase2_sandbox/sandbox_demos/control_flow.c \
					 src/phase2_sandbox/sandbox_demos/control_flow_asm.S \
                     $(SANDBOX_SRC) \
                     $(COMMON_SRC)

FUZZ_CONTROL_FLOW_SRCS := src/phase2_sandbox/fuzzer_control_flow.c \
					 src/phase2_sandbox/sandbox_demos/control_flow_asm.S \
                     $(SANDBOX_SRC) \
                     $(COMMON_SRC)

SIMD_DEMO_SRCS	:=	src/phase2_sandbox/sandbox_demos/simd_template.c	\
					src/phase2_sandbox/sandbox_demos/simd_template_asm.S \
					$(SANDBOX_SRC) \
					$(COMMON_SRC)

FUZZ_SIMD_SRCS		:=	src/phase2_sandbox/fuzzer_simd.c		\
					src/phase2_sandbox/sandbox_demos/simd_template_asm.S \
					$(SANDBOX_SRC) \
					$(COMMON_SRC)


TEST			?= 0xe1a00001

.PHONY: all clean $(MACRO_VALID)

all:	$(DISPATCHER) $(WORKER) $(MACRO_VALID) $(REGS_DEMO) $(PMU_DEMO) $(FUZZ_ARITHMETIC) $(FUZZ_MEMACCESS) $(CONTROL_FLOW_DEMO) $(FUZZ_CONTROL_FLOW) $(SIMD_DEMO) $(FUZZ_SIMD)

$(DISPATCHER): CFLAGS += -DNUM_CORES=$(NUM_CORES)

$(DISPATCHER): $(DISPATCHER_SRCS)
	$(CC) $(CFLAGS) $^ -o $(DISPATCHER)

$(WORKER): $(WORKER_SRCS)
	$(CC) $(CFLAGS) $^ -o $(WORKER)

$(MACRO_VALID):	$(MACRO_SRCS)
	$(CC) $(CFLAGS) -DTEST_INSTRUCTION=$(TEST) $< -o $(MACRO_VALID)

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

$(REGS_DEMO): $(REGS_DOBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(PMU_DEMO): $(PMU_DSRCS)
	$(CC) $(CFLAGS) $^ -o $(PMU_DEMO)

$(FUZZ_ARITHMETIC): $(FUZZ_ARITHMETIC_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(FUZZ_MEMACCESS): $(FUZZ_MEMACCESS_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(CONTROL_FLOW_DEMO): $(CONTROL_FLOW_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(FUZZ_CONTROL_FLOW): $(FUZZ_CONTROL_FLOW_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(SIMD_DEMO): $(SIMD_DEMO_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(FUZZ_SIMD): $(FUZZ_SIMD_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(DISPATCHER) $(WORKER) $(MACRO_VALID) $(REGS_DEMO) $(PMU_DEMO) $(FUZZ_ARITHMETIC) $(FUZZ_MEMACCESS) $(CONTROL_FLOW_DEMO) $(FUZZ_CONTROL_FLOW) $(SIMD_DEMO) $(FUZZ_SIMD)
	rm -f src/phase2_sandbox/sandbox_demos/*.o src/core/*.o src/phase2_sandbox/*.o

$(filter 0x%,$(MAKECMDGOALS)):
	$(MAKE) $(MACRO_VALID) TEST=$@

%:
	@: