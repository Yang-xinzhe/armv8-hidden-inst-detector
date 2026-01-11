ARCH			?=	arm

ifeq ($(ARCH), aarch64)
CC				:=	aarch64-linux-gnu-gcc
CFLAGS			:=	-std=c11 -Wall -Wextra -O0 \
					-march=armv8-a -fomit-frame-pointer \
					-Iinc \
					-Isrc/phase2_sandbox/tests/runner
else
CC				:=	arm-linux-gnueabihf-gcc
CFLAGS			:=	-std=c11 -Wall -Wextra -O0 \
					-march=armv8-a -mfpu=vfpv4 -fomit-frame-pointer -mfloat-abi=hard \
					-Iinc \
					-Isrc/phase2_sandbox/tests/runner
endif

# ---------------------------------------------------------------------------
# Mode Selection: a32 (default), t32_16, t32_32
# ---------------------------------------------------------------------------
MODE ?= a32
ASM_SUFFIX := a32
BIN_SUFFIX := _a32
ARCH_FLAG  := -marm

ifeq ($(MODE), t32_16)
    CFLAGS += -DTEST_T32_16BIT
    ASM_SUFFIX := t32_16
    BIN_SUFFIX := _t32_16
	ARCH_FLAG  := -mthumb
else ifeq ($(MODE), t32_32)
    CFLAGS += -DTEST_T32_32BIT
    ASM_SUFFIX := t32_32
    BIN_SUFFIX := _t32_32
	ARCH_FLAG  := -mthumb
endif

CFLAGS += $(ARCH_FLAG)
# ---------------------------------------------------------------------------
# Directories & Paths
# ---------------------------------------------------------------------------
NUM_CORES		?=	4

LDLIBS          :=  -lpthread -lrt -static

BUILD_DIR   	:= 	build
DEMO_DIR		:=  $(BUILD_DIR)/demo

# Phase 2 Source Roots
SANDBOX_ROOT    := src/phase2_sandbox
HARNESS_ROOT    := $(SANDBOX_ROOT)/harness
TESTS_ROOT      := $(SANDBOX_ROOT)/tests
DEMOS_ROOT      := $(SANDBOX_ROOT)/demos
UTILS_ROOT      := $(SANDBOX_ROOT)/utils
RUNNER_ROOT     := $(TESTS_ROOT)/runner

# Common Sources
COMMON_SRC		:= src/core/cpu_affinity.c 									\
				   src/core/bitmap.c											\
				   src/core/config.c											\
				   src/core/fs_utils.c
SANDBOX_SRC 	:= src/core/sandbox.c
PMU_COUNTER_SRC := src/core/pmu_counter.c
TEST_RUNNER_SRC := $(RUNNER_ROOT)/test_runner.c

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

# Phase 1 Targets
DISPATCHER		:=	$(BUILD_DIR)/dispatcher
WORKER 			:=  $(BUILD_DIR)/worker$(BIN_SUFFIX)

# Phase 2 Utils
INLINE_ASM_VALIDATOR :=	$(BUILD_DIR)/inline_asm_validator

# Phase 2 Demos
DEMO_REGS		:=	$(DEMO_DIR)/demo_regs$(BIN_SUFFIX)
DEMO_PMU		:=	$(DEMO_DIR)/demo_pmu$(BIN_SUFFIX)
DEMO_SIMD		:=	$(DEMO_DIR)/demo_simd$(BIN_SUFFIX)
DEMO_CTRL_FLOW  :=  $(DEMO_DIR)/demo_control_flow$(BIN_SUFFIX)

# Phase 2 Tests (Functional Tests)
TEST_ARITHMETIC 	:=	$(BUILD_DIR)/test_arithmetic$(BIN_SUFFIX)
TEST_MEMORY		    :=	$(BUILD_DIR)/test_memory$(BIN_SUFFIX)
TEST_SIMD			:=	$(BUILD_DIR)/test_simd$(BIN_SUFFIX)
TEST_CONTROL_FLOW 	:= 	$(BUILD_DIR)/test_control_flow$(BIN_SUFFIX)
TEST_CANARY       	:= 	$(BUILD_DIR)/test_canary$(BIN_SUFFIX)

# ---------------------------------------------------------------------------
# Source Lists (Dynamic based on ASM_SUFFIX)
# ---------------------------------------------------------------------------

DISPATCHER_SRCS	:= src/phase1_screening/dispatcher_screen.c 				\
				   $(COMMON_SRC)

WORKER_SRCS		:= src/phase1_screening/worker_screen.c						\
				   $(SANDBOX_SRC)											\
				   $(COMMON_SRC)

VALIDATOR_SRCS	:= $(UTILS_ROOT)/inline_asm_validator.c

# Harness Assembly Selection
HARNESS_REGS_ASM   := $(HARNESS_ROOT)/regs/entry_$(ASM_SUFFIX).S
HARNESS_PMU_ASM    := $(HARNESS_ROOT)/pmu/entry_$(ASM_SUFFIX).S
HARNESS_SIMD_ASM   := $(HARNESS_ROOT)/simd/entry_$(ASM_SUFFIX).S
HARNESS_CTRL_ASM   := $(HARNESS_ROOT)/control_flow/entry_$(ASM_SUFFIX).S
HARNESS_CANARY_ASM := $(HARNESS_ROOT)/canary/entry_$(ASM_SUFFIX).S

# Demos Sources
DEMO_REGS_SRCS     := $(DEMOS_ROOT)/demo_regs.c $(SANDBOX_SRC) $(HARNESS_REGS_ASM)
DEMO_PMU_SRCS      := $(DEMOS_ROOT)/demo_pmu.c $(SANDBOX_SRC) $(PMU_COUNTER_SRC) $(HARNESS_PMU_ASM)
DEMO_SIMD_SRCS     := $(DEMOS_ROOT)/demo_simd.c $(SANDBOX_SRC) $(COMMON_SRC) $(HARNESS_SIMD_ASM)
DEMO_CTRL_SRCS     := $(DEMOS_ROOT)/demo_control_flow.c $(SANDBOX_SRC) $(COMMON_SRC) $(HARNESS_CTRL_ASM)

# Tests Sources (Added TEST_RUNNER_SRC)
TEST_ARITHMETIC_SRCS := $(TESTS_ROOT)/test_arithmetic.c \
                        $(TEST_RUNNER_SRC) \
                        $(SANDBOX_SRC) \
                        $(HARNESS_REGS_ASM) \
                        $(COMMON_SRC)

TEST_MEMORY_SRCS     := $(TESTS_ROOT)/test_memory.c \
                        $(TEST_RUNNER_SRC) \
                        $(PMU_COUNTER_SRC) \
                        $(SANDBOX_SRC) \
                        $(COMMON_SRC) \
                        $(HARNESS_PMU_ASM)

TEST_SIMD_SRCS       := $(TESTS_ROOT)/test_simd.c \
                        $(TEST_RUNNER_SRC) \
                        $(SANDBOX_SRC) \
                        $(COMMON_SRC) \
                        $(HARNESS_SIMD_ASM)

TEST_CTRL_SRCS       := $(TESTS_ROOT)/test_control_flow.c \
                        $(TEST_RUNNER_SRC) \
                        $(SANDBOX_SRC) \
                        $(COMMON_SRC) \
                        $(HARNESS_CTRL_ASM)

TEST_CANARY_SRCS     := $(TESTS_ROOT)/test_canary.c \
                        $(TEST_RUNNER_SRC) \
                        $(SANDBOX_SRC) \
                        $(COMMON_SRC) \
                        $(HARNESS_CANARY_ASM)

# ---------------------------------------------------------------------------
# Build Rules
# ---------------------------------------------------------------------------

TEST			?= 0xe1a00001

.PHONY: all clean $(INLINE_ASM_VALIDATOR)

all:	$(DISPATCHER) $(WORKER)  \
		$(DEMO_REGS) $(DEMO_PMU) $(DEMO_SIMD) $(DEMO_CTRL_FLOW) \
		$(TEST_ARITHMETIC) $(TEST_MEMORY) $(TEST_SIMD) $(TEST_CONTROL_FLOW) $(TEST_CANARY) $(INLINE_ASM_VALIDATOR)

$(BUILD_DIR):
	mkdir -p $@

$(DEMO_DIR): | $(BUILD_DIR)
	mkdir -p $@

$(DISPATCHER): CFLAGS += -DNUM_CORES=$(NUM_CORES)
$(DISPATCHER): $(DISPATCHER_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $(DISPATCHER) $(LDLIBS)

$(WORKER): $(WORKER_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $(WORKER) $(LDLIBS)

$(INLINE_ASM_VALIDATOR): $(VALIDATOR_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DTEST_INSTRUCTION=$(TEST) $< -o $(INLINE_ASM_VALIDATOR) $(LDLIBS)

# Compile Assembly
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Demos
$(DEMO_REGS): $(DEMO_REGS_SRCS) | $(DEMO_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(DEMO_PMU): $(DEMO_PMU_SRCS) | $(DEMO_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(DEMO_SIMD): $(DEMO_SIMD_SRCS) | $(DEMO_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(DEMO_CTRL_FLOW): $(DEMO_CTRL_SRCS) | $(DEMO_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# Tests
$(TEST_ARITHMETIC): $(TEST_ARITHMETIC_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_MEMORY): $(TEST_MEMORY_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_SIMD): $(TEST_SIMD_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_CONTROL_FLOW): $(TEST_CTRL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_CANARY): $(TEST_CANARY_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(DISPATCHER) $(WORKER) $(INLINE_ASM_VALIDATOR)
	rm -f $(DEMO_DIR)/* $(BUILD_DIR)/test_*
	rm -f src/phase2_sandbox/harness/*/*.o src/core/*.o
	rm -rf $(BUILD_DIR)

$(filter 0x%,$(MAKECMDGOALS)):
	$(MAKE) $(INLINE_ASM_VALIDATOR) TEST=$@

%:
	@:
