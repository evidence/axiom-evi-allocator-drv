
COMMKFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

ifeq ($(FS),x86)
  undefine CCARCH
else
  CCARCH := aarch64
endif

BUILDROOT := ${COMMKFILE_DIR}/../axiom-evi-buildroot
OUTPUT_DIR := ${BUILDROOT}/../output
DESTDIR := $(realpath ${OUTPUT_DIR}/target)

ifeq ($(KERN),br)
    CCPREFIX := ${OUTPUT_DIR}/host/usr/bin/$(CCARCH)-linux-
    CROSS_COMPILE := ARCH=arm64 CROSS_COMPILE=$(CCPREFIX)
    KERNELDIR := ${OUTPUT_DIR}/build/linux-custom
    CC := $(OUTPUT_DIR)/host/usr/bin/aarch64-buildroot-linux-gnu-gcc
else
ifeq ($(KERN),x86)
    ifeq ($(FS),x86)
	    KERNELVER :=  $(shell uname -r)
    else
	    KERNELVER :=  $(shell uname -r)
    endif
    KERNELDIR := /lib/modules/$(KERNELVER)/build
else
    CROSS_COMPILE := ARCH=arm64 CROSS_COMPILE=$(PETALINUX)/tools/linux-i386/aarch64-linux-gnu/bin/aarch64-linux-gnu-
    KERNELDIR := $(AXIOMBSP)/build/linux/kernel/link-to-kernel-build
    CC := $(PETALINUX)/tools/linux-i386/aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
endif
endif
