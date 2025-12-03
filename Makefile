CC				:=	arm-linux-gnueabihf-gcc

NUM_CORES		?=	4

CFLAGS			:=	-std=c11 -Wall -Wextra -O2 \
           			-marm -march=armv8-a -mfpu=vfpv4 -fomit-frame-pointer -mfloat-abi=hard \
           			-Iinc \
					-DNUM_CORES=$(NUM_CORES)

DISPATCHER		:=	dispatcher
WORKER			:=	worker
MACRO_VALID		:=	macro_valid


COMMON_SRC		:= src/core/cpu_affinity.c	src/core/sandbox.c src/core/bitmap.c
DISPATCHER_SRCS	:= src/phase1_screening/dispatcher_screen.c $(COMMON_SRC)
WORKER_SRCS		:= src/phase1_screening/worker_screen.c		$(COMMON_SRC)

MACRO_SRCS		:= src/phase2_sandbox/macro_valid.c
TEST			?= 0xe1a00001

.PHONY: all clean $(MACRO_VALID)

all:	$(DISPATCHER) $(WORKER) $(MACRO_VALID)

$(DISPATCHER): $(DISPATCHER_SRCS)
	$(CC) $(CFLAGS) $^ -o $(DISPATCHER)

$(WORKER): $(WORKER_SRCS)
	$(CC) $(CFLAGS) $^ -o $(WORKER)

$(MACRO_VALID):	$(MACRO_SRCS)
	$(CC) $(CFLAGS) -DTEST_INSTRUCTION=$(TEST) $< -o $(MACRO_VALID)

clean:
	rm -f $(DISPATCHER) $(WORKER) $(MACRO_VALID)

$(filter 0x%,$(MAKECMDGOALS)):
	$(MAKE) $(MACRO_VALID) TEST=$@

%:
	@: