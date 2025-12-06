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


COMMON_SRC		:= src/core/cpu_affinity.c 									\
				   src/core/bitmap.c

SANDBOX_SRC 	:= src/core/sandbox.c

REGS_TEMPLATE_SRC := src/phase2_sandbox/sandbox_demos/regs_template.S

DISPATCHER_SRCS	:= src/phase1_screening/dispatcher_screen.c 				\
				   $(COMMON_SRC)

WORKER_SRCS		:= src/phase1_screening/worker_screen.c						\
				   $(SANDBOX_SRC)											\
				   $(COMMON_SRC)

MACRO_SRCS		:= src/phase2_sandbox/macro_valid.c

REGS_DSRCS		:= src/phase2_sandbox/sandbox_demos/regs_diff.c 			\
                   $(SANDBOX_SRC) 											\
                   $(REGS_TEMPLATE_SRC)
REGS_DOBJS 		:= $(REGS_DSRCS:.c=.o)
REGS_DOBJS 		:= $(REGS_DOBJS:.S=.o)

PMU_DSRCS		:= src/phase2_sandbox/sandbox_demos/lsu_pmu.c				\
				   src/core/pmu_counter.c									\
				   $(SANDBOX_SRC)											\
				   $(REGS_TEMPLATE_SRC)			
				   
				   


TEST			?= 0xe1a00001

.PHONY: all clean $(MACRO_VALID)

all:	$(DISPATCHER) $(WORKER) $(MACRO_VALID) $(REGS_DEMO) $(PMU_DEMO)

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

clean:
	rm -f $(DISPATCHER) $(WORKER) $(MACRO_VALID) $(REGS_DEMO) $(PMU_DEMO)
	rm -f src/phase2_sandbox/sandbox_demos/*.o src/core/*.o

$(filter 0x%,$(MAKECMDGOALS)):
	$(MAKE) $(MACRO_VALID) TEST=$@

%:
	@: