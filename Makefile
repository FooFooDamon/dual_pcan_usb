# SPDX-License-Identifier: GPL-2.0

#
# Root Makefile to make DKMS work.
#
# Copyright (c) 2023-2024 Man Hung-Coeng <udc577@126.com>
#

TARGETS := all clean install uninstall update prepare
.PHONY: ${TARGETS}

${TARGETS}: %:
	${MAKE} $@ -C src

