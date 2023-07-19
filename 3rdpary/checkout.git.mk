#!/usr/bin/make -f

#
# Different ways of checking out a project over Git.
#
# Copyright (c) 2023 Man Hung-Coeng <udc577@126.com>
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

ifeq ($(strip ${VCS}),git)

# Q is short for "quiet".
Q := $(if $(strip $(filter-out n N no NO No 0, ${V} ${VERBOSE})),,@)

export CHKOUT_PARENT_DIR ?= $(abspath .)
export CHKOUT_ALIAS ?= lazy_coding
# A tag that is usually created for a formal release.
export CHKOUT_TAG ?=
# A hash code that is generated on each commit.
export CHKOUT_HASH ?=
# Default branch, which is usually "main" or "master".
export CHKOUT_STEM ?= main
export CHKOUT_URL ?= https://github.com/FooFooDamon/lazy_coding_skills
export CHKOUT_TAIL_PARAMS ?=
export CHKOUT_METHOD ?= partial
export CHKOUT_PARTIAL_ITEMS ?= main/makefile/__ver__.mk \
    dcaa17d6c48ec5baf30fe2602c9032bfa144bed7/makefile/c_and_cpp.mk \
    # Add more items ahead of this line if needed. \
    # Beware that each line should begin with 4 spaces and end with a backslash.

.PHONY: checkout checkout-partial checkout-all checkout-newest \
    checkout-by-tag checkout-by-tag-once \
    checkout-by-hash checkout-by-hash-once

checkout: checkout-${CHKOUT_METHOD}

checkout-partial:
	${Q}mkdir -p ${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS} && cd ${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}; \
	$(if ${Q},,set -x;) \
	[ -d .git ] || for i in ${CHKOUT_PARTIAL_ITEMS}; \
	do \
		mkdir -p ".paths/$$(dirname $${i})" "$$(dirname $${i#*/})"; \
		[ -s .paths/$${i} ] && continue || :; \
		$(if ${Q},printf "WGET\t$${i}\n";) \
		wget $(if ${Q},-q) -c -O .paths/$${i} "${CHKOUT_URL}/raw/$${i}${CHKOUT_TAIL_PARAMS}" \
			&& ln -sf "$$(realpath .paths/$${i})" "$$(dirname $${i#*/})/$${i##*/}"; \
	done

checkout-all:
	${Q}mkdir -p ${CHKOUT_PARENT_DIR} && cd ${CHKOUT_PARENT_DIR}; \
	$(if ${Q},,set -x;) \
	if [ ! -d ${CHKOUT_ALIAS} ]; then \
		$(if ${Q},printf 'CLONE\t${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}:ALL\n';) \
		git clone $(if ${Q},-q) '${CHKOUT_URL}' ${CHKOUT_ALIAS}; \
	fi

checkout-newest:
	${Q}mkdir -p ${CHKOUT_PARENT_DIR} && cd ${CHKOUT_PARENT_DIR}; \
	$(if ${Q},,set -x;) \
	if [ -d ${CHKOUT_ALIAS} ]; then \
		$(if ${Q},printf 'UPDATE\t${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}\n';) \
		git -C ${CHKOUT_ALIAS} pull $(if ${Q},-q); \
	else \
		$(if ${Q},printf 'CLONE\t${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}:NEWEST\n';) \
		git clone $(if ${Q},-q) --depth=1 '${CHKOUT_URL}' ${CHKOUT_ALIAS}; \
	fi

checkout-by-tag:
	@if [ -z '${CHKOUT_TAG}' ]; then \
		echo "*** CHKOUT_TAG not specified!" >&2; \
		exit 1; \
	fi
	${Q}[ -d ${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS} ] || ${MAKE} $(if ${Q},-s) checkout-all
	${Q}export GIT='git -C ${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}'; \
	$${GIT} describe --always > /dev/null; \
	$(if ${Q},,set -x;) \
	[ "$$($${GIT} log --pretty=format:%D -n 1 | grep 'tag: ' | sed 's/.*tag: \(.*\)/\1/')" != "${CHKOUT_TAG}" ] \
		|| exit 0; \
	if [ "${CHKOUT_TAG}" != "${CHKOUT_STEM}" ]; then \
		$${GIT} checkout $(if ${Q},-q) '${CHKOUT_STEM}'; \
		[ $$($${GIT} tag | grep -c '^${CHKOUT_TAG}$$') -gt 0 ] \
			|| ${MAKE} $(if ${Q},-s) checkout-newest; \
	fi; \
	$(if ${Q},printf 'SWITCH\t${CHKOUT_TAG}\n';) \
	$${GIT} checkout $(if ${Q},-q) '${CHKOUT_TAG}'

checkout-by-tag-once:
	@if [ -z '${CHKOUT_TAG}' ]; then \
		echo "*** CHKOUT_TAG not specified!" >&2; \
		exit 1; \
	fi
	${Q}mkdir -p ${CHKOUT_PARENT_DIR} && cd ${CHKOUT_PARENT_DIR}; \
	$(if ${Q},,set -x;) \
	if [ ! -d ${CHKOUT_ALIAS} ]; then \
		$(if ${Q},printf 'CLONE\t${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}:${CHKOUT_TAG}\n';) \
		git clone $(if ${Q},-q) -b '${CHKOUT_TAG}' --depth=1 '${CHKOUT_URL}' ${CHKOUT_ALIAS}; \
	fi

checkout-by-hash:
	@if [ -z '${CHKOUT_HASH}' ]; then \
		echo "*** CHKOUT_HASH not specified!" >&2; \
		exit 1; \
	fi
	${Q}[ -d ${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS} ] || ${MAKE} $(if ${Q},-s) checkout-all
	${Q}export GIT='git -C ${CHKOUT_PARENT_DIR}/${CHKOUT_ALIAS}'; \
	$${GIT} describe --always > /dev/null; \
	$(if ${Q},,set -x;) \
	if [ "${CHKOUT_HASH}" != "${CHKOUT_STEM}" ]; then \
		$${GIT} checkout $(if ${Q},-q) '${CHKOUT_STEM}'; \
		[ $$($${GIT} log --pretty=format:%H | grep -c '${CHKOUT_HASH}') -gt 0 ] \
			|| ${MAKE} $(if ${Q},-s) checkout-newest; \
	fi; \
	$(if ${Q},printf 'SWITCH\t${CHKOUT_HASH}\n';) \
	$${GIT} checkout $(if ${Q},-q) '${CHKOUT_HASH}'

checkout-by-hash-once:
	@echo "*** ${@}: Operation not supported yet!" >&2
	@exit 1

endif # ifeq ($(strip ${VCS}),git)

#
# ================
#   CHANGE LOG
# ================
#
# >>> 2023-07-02, Man Hung-Coeng <udc577@126.com>:
#   01. Create.
#

