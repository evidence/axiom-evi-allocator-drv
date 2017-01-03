
COMMKFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CCARCH := aarch64

BUILDROOT := ${COMMKFILE_DIR}/../axiom-evi-buildroot
OUTPUT_DIR := ${BUILDROOT}/../output
DESTDIR := $(realpath ${OUTPUT_DIR}/target)

ifdef CCARCH
    KERNELDIR := ${OUTPUT_DIR}/build/linux-custom
    CCPREFIX := ${OUTPUT_DIR}/host/usr/bin/$(CCARCH)-linux-
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
