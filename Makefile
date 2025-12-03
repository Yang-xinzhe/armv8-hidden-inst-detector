CC				:=	arm-linux-gnueabihf-gcc

NUM_CORES		?=	4

CFLAGS			:=	-std=c11 -Wall -Wextra -O2 \
           			-marm -march=armv8-a -mfpu=vfpv4 -fomit-frame-pointer -mfloat-abi=hard \
           			-Iinc \
					-DNUM_CORES=$(NUM_CORES)

DISPATCHER		:=	dispatcher
WORKER			:=	worker


COMMON_SRC		:= src/core/cpu_affinity.c	src/core/sandbox.c src/core/bitmap.c
DISPATCHER_SRCS	:= src/phase1_screening/dispatcher_screen.c $(COMMON_SRC)
WORKER_SRCS		:= src/phase1_screening/worker_screen.c		$(COMMON_SRC)

.PHONY: all clean

all:	$(DISPATCHER) $(WORKER)

$(DISPATCHER): $(DISPATCHER_SRCS)
	$(CC) $(CFLAGS) $^ -o $(DISPATCHER)

$(WORKER): $(WORKER_SRCS)
	$(CC) $(CFLAGS) $^ -o $(WORKER)

clean:
	rm -f $(DISPATCHER) $(WORKER)

