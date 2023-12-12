#!/usr/bin/make -f
# SPDX-License-Identifier: GPL-2.0

#
# Global Makefile for all components.
#
# Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
# All rights reserved.
#




# Make sure that "all" is the first and the only target in this file.
all: init




LCS_ALIAS := lazy_coding
export THIRD_PARTY_DIR ?= $(abspath ../3rdpary)
EVAL_VERSION_ONCE := Y

-include ${THIRD_PARTY_DIR}/${LCS_ALIAS}/makefile/__ver__.mk




# export CROSS_KERNEL_DIR := ${HOME}/src/linux
#export DRVNAME ?= dual_pcan_usb
export DRVNAME ?= pcan
export ${DRVNAME}-objs ?= main.o usb_driver.o can_commands.o \
    packet_codec.o netdev_interfaces.o chardev_interfaces.o \
    chardev_ioctl.o \
    ../3rdpary/lazy_coding/c_and_cpp/native/chardev_group.o
ccflags-y += -I${THIRD_PARTY_DIR}/${LCS_ALIAS}/c_and_cpp/native
ifneq ($(filter-out n N no NO No 0, ${INNER_TEST}),)
    ccflags-y += -DINNER_TEST
endif
export USE_SRC_RELATIVE_PATH := 1
export APP_NAME := ${DRVNAME}
export APP_OBJS := setting_app.o
export APP_INCLUDES := -I${THIRD_PARTY_DIR}/${LCS_ALIAS}/c_and_cpp/native
export OTHER_APP_CFLAGS := -std=c99

-include ${THIRD_PARTY_DIR}/${LCS_ALIAS}/makefile/linux_driver.mk




VCS_LIST := git
VCS ?= $(word 1, ${VCS_LIST})
LCS_URL := https://github.com/FooFooDamon/lazy_coding_skills
# Format of each project item: <alias>@@<method>@@<vcs>@@<default-branch>@@<url>
# NOTES:
# 1) If the method field of an item is by-tag or by-hash,
#   then its tag or hash code needs to be set into file manually
#   after "make seeds" and before "make init".
# 2) The "partial" method only works in HTTP(S) way so far.
# 3) SVN projects are not supported yet.
THIRD_PARTY_PROJECTS := ${LCS_ALIAS}@@partial@@git@@main@@${LCS_URL} \
    # Add more items ahead of this line if needed. \
    # Beware that each line should begin with 4 spaces and end with a backslash.
CHKOUT ?= ${LCS_ALIAS}
CHKOUT_CONF_DIR := $(abspath ../__chkout__)

seeds:
	$(if ${Q},@printf '>>> SEEDS: Begin.\n')
	${Q}mkdir -p ${THIRD_PARTY_DIR} ${CHKOUT_CONF_DIR}
	${Q}for i in ${VCS_LIST}; \
	do \
		[ ! -s ${THIRD_PARTY_DIR}/checkout.$${i}.mk ] || continue; \
		$(if ${Q},printf "WGET\t${THIRD_PARTY_DIR}/checkout.$${i}.mk\n";) \
		wget $(if ${Q},-q) -c -O ${THIRD_PARTY_DIR}/checkout.$${i}.mk "${LCS_URL}/raw/main/makefile/checkout.$${i}.mk"; \
	done
	${Q}for i in ${THIRD_PARTY_PROJECTS}; \
	do \
		export CHKOUT_ALIAS=$$(echo "$${i}" | awk -F '@@' '{ print $$1 }'); \
		export CHKOUT_METHOD=$$(echo "$${i}" | awk -F '@@' '{ print $$2 }'); \
		export VCS_CMD=$$(echo "$${i}" | awk -F '@@' '{ print $$3 }'); \
		export CHKOUT_STEM=$$(echo "$${i}" | awk -F '@@' '{ print $$4 }'); \
		export CHKOUT_URL=$$(echo "$${i}" | awk -F '@@' '{ print $$5 }'); \
		export MKFILE=${CHKOUT_CONF_DIR}/$${CHKOUT_ALIAS}.$${VCS_CMD}.chkout.mk; \
		[ ! -e $${MKFILE} ] || continue; \
		echo "# It's better to use a relative path in a project under versioning control," > $${MKFILE}; \
		echo "# or define this variable in absolute path through command line parameter." >> $${MKFILE}; \
		echo "#export CHKOUT_PARENT_DIR := ${THIRD_PARTY_DIR}" >> $${MKFILE}; \
		echo "export CHKOUT_ALIAS := $${CHKOUT_ALIAS}" >> $${MKFILE}; \
		echo "export CHKOUT_METHOD := $${CHKOUT_METHOD}" >> $${MKFILE}; \
		echo "export CHKOUT_TAG :=" >> $${MKFILE}; \
		echo "export CHKOUT_HASH :=" >> $${MKFILE}; \
		echo "export CHKOUT_STEM := $${CHKOUT_STEM}" >> $${MKFILE}; \
		echo "export CHKOUT_URL := $${CHKOUT_URL}" >> $${MKFILE}; \
		echo "export CHKOUT_TAIL_PARAMS :=" >> $${MKFILE}; \
		if [ "$${CHKOUT_ALIAS}" = "${LCS_ALIAS}" ]; then \
			echo "export CHKOUT_PARTIAL_ITEMS := main/makefile/__ver__.mk \\" >> $${MKFILE}; \
			echo "    adfce11edd4fa36f324f0e1ac6d4396341cc5266/makefile/c_and_cpp.mk \\" >> $${MKFILE}; \
			echo "    main/makefile/linux_driver.mk \\" >> $${MKFILE}; \
			echo "    main/makefile/stm32_cube_ide.mk \\" >> $${MKFILE}; \
		else \
			echo "export CHKOUT_PARTIAL_ITEMS := \\" >> $${MKFILE}; \
		fi; \
		echo "    # Add more items ahead of this line if needed. \\" >> $${MKFILE}; \
		echo "    # Beware that each line should begin with 4 spaces and end with a backslash." >> $${MKFILE}; \
		echo "" >> $${MKFILE}; \
		echo "[$${MKFILE}] has been created. Edit it properly before use."; \
	done
	$(if ${Q},@printf '>>> SEEDS: Done.\n')

chkout-exec:
	${Q}for i in ${VCS_LIST}; \
	do \
		[ ! -s ${THIRD_PARTY_DIR}/checkout.$${i}.mk ] || continue; \
		echo "*** [${THIRD_PARTY_DIR}/checkout.$${i}.mk] is empty, or does not exist!" >&2; \
		echo '*** Run "make seeds" to check it out first!' >&2; \
		exit 1; \
	done
	${Q}for i in ${THIRD_PARTY_PROJECTS}; \
	do \
		export CHKOUT_ALIAS=$$(echo "$${i}" | awk -F '@@' '{ print $$1 }'); \
		export VCS_CMD=$$(echo "$${i}" | awk -F '@@' '{ print $$3 }'); \
		export MKFILE=${CHKOUT_CONF_DIR}/$${CHKOUT_ALIAS}.$${VCS_CMD}.chkout.mk; \
		if [ ! -e $${MKFILE} ]; then \
			echo "*** [$${MKFILE}] does not exist!" >&2; \
			echo '*** Run "make seeds" to create it first!' >&2; \
			exit 1; \
		fi; \
		ask_and_quit() { echo "*** Have you modified [$${MKFILE}] correctly ?!" >&2; exit 1; }; \
		$(if ${Q},printf ">>> CHKOUT: Begin checking out [$${CHKOUT_ALIAS}].\n";) \
		${MAKE} $(if ${Q},-s) checkout VCS=$${VCS_CMD} CHKOUT=$${CHKOUT_ALIAS} CHKOUT_PARENT_DIR=${THIRD_PARTY_DIR} \
			|| ask_and_quit; \
		$(if ${Q},printf ">>> CHKOUT: Done checking out [$${CHKOUT_ALIAS}].\n";) \
	done

-include ${CHKOUT_CONF_DIR}/${CHKOUT}.${VCS}.chkout.mk
-include ${THIRD_PARTY_DIR}/checkout.${VCS}.mk

pre-init:
	${Q}:

post-init:
	${Q}:

init: pre-init chkout-exec post-init




#
# If you want to re-define commands of some targets, write them here:
#

# FIXME: More rules if needed ...

