# SPDX-License-Identifier: GPL-2.0

#
# Makefile wrapper for out-of-tree Linux driver.
#
# Copyright (c) 2023-2024 Man Hung-Coeng <udc577@126.com>
#

.PHONY: all prepare dependencies

# NOTE: Some paths must be absolute paths while some mustn't, and the rest don't care.
LAZY_CODING_DIR := ../3rdparty/lazy_coding
export LAZY_CODING_ABSDIR ?= $(abspath ${LAZY_CODING_DIR})
export LAZY_CODING_MAKEFILES ?= $(addprefix ${LAZY_CODING_ABSDIR}/makefile/, __ver__.mk linux_driver.mk)

ifeq ($(shell [ true $(foreach i, ${LAZY_CODING_MAKEFILES}, -a -s ${i}) ] && echo 1 || echo 0),0)

LAZY_CODING_URL ?= https://github.com/FooFooDamon/lazy_coding_skills

all prepare: dependencies
	@for i in ${LAZY_CODING_MAKEFILES}; \
	do \
		mkdir -p $$(dirname $${i}); \
		[ -s $${i} ] || wget -c -O $${i} "${LAZY_CODING_URL}/raw/main/makefile/$$(basename $${i})"; \
	done
	@echo "~ ~ ~ Minimum preparation finished successfully ~ ~ ~"
	@echo "Re-run your command again to continue your work."

else

all: dependencies

export EVAL_VERSION_ONCE ?= Y
export NO_PRIV_STUFF := $(strip $(filter-out n N no NO No 0, ${NO_PRIV_STUFF}))

# export ARCH := aarch64
# export HOST_KERNEL_DIR := /lib/modules/`uname -r`/build
# export CROSS_KERNEL_DIR := ${HOME}/src/linux
export DRVNAME ?= pcan
export ${DRVNAME}-objs ?= main.o usb_driver.o can_commands.o \
    packet_codec.o netdev_operations.o chardev_operations.o \
    chardev_ioctl.o chardev_sysfs.o \
    $(addprefix ${LAZY_CODING_DIR}/c_and_cpp/native/, chardev_group.o devclass_supplements.o)
export USE_SRC_RELATIVE_PATH ?= 1
ccflags-y += -I${LAZY_CODING_ABSDIR}/c_and_cpp/native
ifneq ($(filter-out n N no NO No 0, ${INNER_TEST}),)
    ccflags-y += -DINNER_TEST
endif
export APP_NAME ?= ${DRVNAME}usb_test
export APP_OBJS ?= test_app.o $(addprefix ${LAZY_CODING_DIR}/c_and_cpp/native/, signal_handling.o)
export APP_INCLUDES := -I${LAZY_CODING_DIR}/c_and_cpp/native
export OTHER_APP_CFLAGS := -std=gnu99

include ${LAZY_CODING_MAKEFILES}

export DKMS_CONF := ../dkms.conf
export DKMS_PKG_NAME := dual_pcan_usb
export DKMS_PKG_VER ?= $(shell \
    grep "\#define[ ]\+DRV_VER_\(MAJOR\|MINOR\|RELEASE\)" versions.h \
    | awk '{ print $$3 }' \
    | sed 'N; N; s/\n/./g' \
)

.PHONY: install uninstall update ${DKMS_CONF} dkms_purge

install: ${DKMS_CONF}
	$(if ${Q},@printf '>>> INSTALL: Begin.\n')
	${Q}[ -f /etc/modprobe.d/${DRVNAME}.conf ] || install -p -m 644 ../conf/${DRVNAME}.conf /etc/modprobe.d/
	${Q}install -p -m 644 ../conf/blacklist-peak_system.conf /etc/modprobe.d/
	${Q}install -p -m 644 ../conf/45-dual_pcan_usb.rules /etc/udev/rules.d/ && udevadm control --reload-rules
	${Q}ln -snf $$(realpath $${PWD}/..) /usr/src/${DKMS_PKG_NAME}-${DKMS_PKG_VER}
	${Q}dkms install --force ${DKMS_PKG_NAME}/${DKMS_PKG_VER} -k $$(uname -r)
	$(if ${Q},@printf '>>> INSTALL: Done.\n')

uninstall:
	$(if ${Q},@printf '>>> UNINSTALL: Begin.\n')
	${Q}rm -f /etc/modprobe.d/${DRVNAME}.conf /etc/modprobe.d/blacklist-peak_system.conf
	${Q}rm -f /etc/udev/rules.d/45-dual_pcan_usb.rules && udevadm control --reload-rules
	${Q}-dkms remove ${DKMS_PKG_NAME}/${DKMS_PKG_VER} --all
	${Q}rm -f /usr/src/${DKMS_PKG_NAME}-${DKMS_PKG_VER}
	$(if ${Q},@printf '>>> UNINSTALL: Done.\n')

update: dkms_purge install

${DKMS_CONF}:
	${Q}echo 'PACKAGE_NAME="${DKMS_PKG_NAME}"' > ${DKMS_CONF}
	${Q}echo 'PACKAGE_VERSION="${DKMS_PKG_VER}"' >> ${DKMS_CONF}
	${Q}echo 'CLEAN="make clean"' >> ${DKMS_CONF}
	${Q}echo "MAKE[0]=\"'make'\"" >> ${DKMS_CONF}
	${Q}echo 'BUILT_MODULE_NAME[0]="${DRVNAME}"' >> ${DKMS_CONF}
	${Q}echo 'BUILT_MODULE_LOCATION[0]="src"' >> ${DKMS_CONF}
	${Q}echo 'DEST_MODULE_LOCATION[0]="/updates"' >> ${DKMS_CONF}
	${Q}echo 'AUTOINSTALL="yes"' >> ${DKMS_CONF}

dkms_purge:
	$(if ${Q},@printf '>>> DKMS-PURGE: Begin.\n')
	${Q}-ls /usr/src/ | grep ${DKMS_PKG_NAME}- | while read i; \
	do \
		ii=$$(echo $${i} | sed 's/\(${DKMS_PKG_NAME}\)-/\1\//'); \
		dkms remove $${ii} --all; \
		rm /usr/src/$${i}; \
	done
	$(if ${Q},@printf '>>> DKMS-PURGE: Done.\n')

endif

export DEPENDENCY_DIRS ?= $(abspath ../3rdparty)

dependencies:
	@for i in ${DEPENDENCY_DIRS}; \
	do \
		[ -s $${i}/[Mm]akefile ] && ${MAKE} $(filter all prepare, ${MAKECMDGOALS}) -C $${i} || true; \
	done

