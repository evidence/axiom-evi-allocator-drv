ifdef CCARCH
    BUILDROOT := ${PWD}/../axiom-evi-buildroot
    KERNELDIR := ${BUILDROOT}/output/build/linux-custom
    DESTDIR := ${BUILDROOT}/output/target
    CCPREFIX := ${BUILDROOT}/output/host/usr/bin/$(CCARCH)-linux-
ifeq ($(CCARCH), aarch64)
    CROSS_COMPILE := ARCH=arm64 CROSS_COMPILE=$(CCPREFIX)
else
    CROSS_COMPILE := ARCH=$(CCARCH) CROSS_COMPILE=$(CCPREFIX)
endif
else
    KERNELDIR := /lib/modules/$(shell uname -r)/build
    CCPREFIX :=
    CROSS_COMPILE :=
endif

axiom_mem_dev-objs := axiom_mem_dev_lib.o axiom_memory_dev.o axiom_mem_manager.o
obj-m += axiom_mem_dev.o

all:
	$(MAKE) $(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules

install: all
	$(MAKE) $(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(DESTDIR) modules_install

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
