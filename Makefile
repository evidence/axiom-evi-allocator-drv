PWD := $(shell pwd)
MKFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

include $(MKFILE_DIR)/common.mk

axiom_memory_manager-objs := axiom_mem_manager.o
obj-m += axiom_mem_manager.o

axiom_mem_dev-objs := axiom_memory_dev.o
obj-m += axiom_mem_dev.o

ifeq ($(KERN),seco)
CC:=$(PETALINUX)/tools/linux-i386/aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
endif

all:
	$(MAKE) $(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) modules

install: all
	$(MAKE) $(CROSS_COMPILE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(DESTDIR) modules_install
	mkdir -p $(DESTDIR)/usr/include/linux
	cp axiom_mem_dev_user.h $(DESTDIR)/usr/include/
	cp axiom_mem_dev.h $(DESTDIR)/usr/include/linux/

clean distclean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
