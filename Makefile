CC				:=	arm-linux-gnueabihf-gcc

NUM_CORES		?=	4

CFLAGS			:=	-std=c11 -Wall -Wextra -O2 \
           			-marm -march=armv8-a -mfpu=vfpv4 -fomit-frame-pointer -mfloat-abi=hard \
           			-Iinc \
					-DNUM_CORES=$(NUM_CORES)

DISPATCHER		:=	dispatcher
WORKER			:=	worker
MACRO_VALID		:=	macro_valid
REGS_DEMO		:= 	regs_demo


COMMON_SRC		:= src/core/cpu_affinity.c src/core/bitmap.c
DISPATCHER_SRCS	:= src/phase1_screening/dispatcher_screen.c $(COMMON_SRC)
WORKER_SRCS		:= src/phase1_screening/worker_screen.c						\
				   src/core/sandbox.c						$(COMMON_SRC)
MACRO_SRCS		:= src/phase2_sandbox/macro_valid.c
REGS_DSRCS		:= src/phase2_sandbox/sandbox_demos/regs_diff.c 			\
                   src/core/sandbox.c 										\
                   src/phase2_sandbox/sandbox_demos/regs_template.S
REGS_DOBJS := $(REGS_DSRCS:.c=.o)
REGS_DOBJS := $(REGS_DOBJS:.S=.o)


TEST			?= 0xe1a00001

.PHONY: all clean $(MACRO_VALID)

all:	$(DISPATCHER) $(WORKER) $(MACRO_VALID) $(REGS_DEMO)

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

clean:
	rm -f $(DISPATCHER) $(WORKER) $(MACRO_VALID) $(REGS_DEMO)
	rm -f $(REGS_DOBJS)
	rm -f $(DISPATCHER_SRCS:.c=.o) $(WORKER_SRCS:.c=.o) $(MACRO_SRCS:.c=.o)

$(filter 0x%,$(MAKECMDGOALS)):
	$(MAKE) $(MACRO_VALID) TEST=$@

%:
	@: