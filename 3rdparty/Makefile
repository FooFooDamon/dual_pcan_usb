#
# For checking out 3rd-party projects.
#
# Copyright (c) 2024 Man Hung-Coeng <udc577@126.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

.PHONY: all prepare seeds

VCS_LIST ?= git
VCS ?= $(word 1, ${VCS_LIST})
CHKOUT_CONF_DIR := $(abspath ./__chkout__)
LAZY_CODING_MAKEFILES := $(foreach i, ${VCS_LIST}, ${CHKOUT_CONF_DIR}/checkout.${i}.mk)
LAZY_CODING_URL := https://github.com/FooFooDamon/lazy_coding_skills
# Q is short for "quiet".
Q := $(if $(strip $(filter-out n N no NO No 0, ${V} ${VERBOSE})),,@)

ifeq ($(shell [ true $(foreach i, ${LAZY_CODING_MAKEFILES}, -a -s ${i}) ] && echo 1 || echo 0),0)

all prepare:
	${Q}for i in ${LAZY_CODING_MAKEFILES}; \
	do \
		mkdir -p $$(dirname $${i}); \
		$(if ${Q},printf "WGET\t$$(basename $${i})\n";) \
		[ -s $${i} ] || wget $(if ${Q},-q) -c -O $${i} "${LAZY_CODING_URL}/raw/main/makefile/$$(basename $${i})"; \
	done
	${Q}echo "~ ~ ~ Minimum preparation finished successfully ~ ~ ~"
	${Q}echo "Re-run your command again to continue your work."

else

LAZY_CODING_ALIAS := lazy_coding
CHKOUT_TARGET ?= ${LAZY_CODING_ALIAS}
# Format of each project item:
#   <alias>@@<method>@@<vcs>@@<default-branch>@@<url>
# NOTES:
# 1) If the method field of an item is by-tag or by-hash,
#   then its tag or hash code needs to be set into file manually
#   after "make seeds".
# 2) The "partial" method only works in HTTP(S) way so far.
# 3) SVN projects are not supported yet.
THIRD_PARTY_PROJECTS := ${LAZY_CODING_ALIAS}@@partial@@git@@main@@${LAZY_CODING_URL} \
    #nvidia-docker-v2.12.0@@by-tag@@git@@main@@https://gitlab.com/nvidia/container-toolkit/nvidia-docker \
    #nvidia-docker-80902fe3afab@@by-hash@@git@@main@@git@gitlab.com:nvidia/container-toolkit/nvidia-docker.git \
    #rt-thread@@by-tag@@git@@master@@https://gitee.com/rtthread/rt-thread.git \
    # FIXME: Add more items ahead of this line if needed. \
    # Beware that each line should begin with 4 spaces and end with a backslash.

all prepare: chkout-exec

chkout-exec:
	${Q}for i in ${THIRD_PARTY_PROJECTS}; \
	do \
		export CHKOUT_ALIAS=$$(echo "$${i}" | awk -F '@@' '{ print $$1 }'); \
		export VCS_CMD=$$(echo "$${i}" | awk -F '@@' '{ print $$3 }'); \
		export MKFILE=${CHKOUT_CONF_DIR}/$${CHKOUT_ALIAS}.$${VCS_CMD}.chkout.mk; \
		if [ ! -e $${MKFILE} ]; then \
			echo "*** [$${MKFILE}] does not exist!" >&2; \
			echo '*** Run "${MAKE} seeds" to create it first!' >&2; \
			exit 1; \
		fi; \
		ask_and_quit() { echo "*** Have you modified [$${MKFILE}] correctly ?!" >&2; exit 1; }; \
		$(if ${Q},printf ">>> CHKOUT: Begin checking out [$${CHKOUT_ALIAS}].\n";) \
		${MAKE} $(if ${Q},-s) checkout VCS=$${VCS_CMD} CHKOUT_TARGET=$${CHKOUT_ALIAS} CHKOUT_PARENT_DIR=$$(pwd) \
			|| ask_and_quit; \
		$(if ${Q},printf ">>> CHKOUT: Done checking out [$${CHKOUT_ALIAS}].\n";) \
	done

seeds:
	$(if ${Q},@printf '>>> SEEDS: Begin.\n')
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
		echo "#export CHKOUT_PARENT_DIR := $$(pwd)" >> $${MKFILE}; \
		echo "export CHKOUT_ALIAS := $${CHKOUT_ALIAS}" >> $${MKFILE}; \
		echo "export CHKOUT_METHOD := $${CHKOUT_METHOD}" >> $${MKFILE}; \
		echo "export CHKOUT_TAG :=" >> $${MKFILE}; \
		echo "export CHKOUT_HASH :=" >> $${MKFILE}; \
		echo "export CHKOUT_STEM := $${CHKOUT_STEM}" >> $${MKFILE}; \
		echo "export CHKOUT_URL := $${CHKOUT_URL}" >> $${MKFILE}; \
		echo "export CHKOUT_TAIL_PARAMS :=" >> $${MKFILE}; \
		if [ "$${CHKOUT_ALIAS}" = "${LAZY_CODING_ALIAS}" ]; then \
			echo "export CHKOUT_PARTIAL_ITEMS := main/makefile/__ver__.mk \\" >> $${MKFILE}; \
			echo "    053ddeb138883b235d783803fd747fc596349071/c_and_cpp/native/__ver__.h \\" >> $${MKFILE}; \
		else \
			echo "export CHKOUT_PARTIAL_ITEMS := \\" >> $${MKFILE}; \
		fi; \
		echo "    # Add more items ahead of this line if needed. \\" >> $${MKFILE}; \
		echo "    # Beware that each line should begin with 4 spaces and end with a backslash." >> $${MKFILE}; \
		echo "" >> $${MKFILE}; \
		echo "[$${MKFILE}] has been created. Edit it properly before use."; \
	done
	$(if ${Q},@printf '>>> SEEDS: Done.\n')

-include ${CHKOUT_CONF_DIR}/${CHKOUT_TARGET}.${VCS}.chkout.mk
include ${CHKOUT_CONF_DIR}/checkout.${VCS}.mk

# FIXME: Add more rules if needed, and delete this comment line then.

endif

