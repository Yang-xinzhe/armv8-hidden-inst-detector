# pmu_user_module/Makefile

obj-m += pmu_user.o


KDIR ?= /lib/modules/$(shell uname -r)/build

# 交叉工具链前缀（下面会讲如何安装）
CROSS_COMPILE ?= aarch64-linux-gnu-
ARCH ?= arm64

all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
