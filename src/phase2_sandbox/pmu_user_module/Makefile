# pmu_user_module/Makefile

obj-m += pmu_user.o

# 这里改成你实际的内核源码路径
KDIR ?= ~/linux

# 交叉工具链前缀（下面会讲如何安装）
CROSS_COMPILE ?= aarch64-linux-gnu-
ARCH ?= arm64

all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
