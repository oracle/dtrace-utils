# Top-level makefile for dtrace.
#
# Build files in subdirectories are included by this file.
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2011 -- 2016 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

.DELETE_ON_ERROR:
.SUFFIXES:
.SECONDEXPANSION:

PROJECT := dtrace
VERSION := 0.6.1

# Verify supported hardware.

$(if $(subst sparc64,,$(subst x86_64,,$(shell uname -m))), \
    $(error "Error: DTrace for Linux only supports x86_64 and sparc64"),)
$(if $(subst Linux,,$(shell uname -s)), \
    $(error "Error: DTrace only supports Linux"),)

CFLAGS ?= -O2 -g -Wall -pedantic -Wno-unknown-pragmas
LDFLAGS ?=
BITNESS := 64
NATIVE_BITNESS_ONLY := $(shell echo 'int main (void) { }' | gcc -x c -o /dev/null -m32 - 2>/dev/null || echo t)
ARCHINC := $(subst sparc64,sparc,$(subst x86_64,i386,$(shell uname -m)))
INVARIANT_CFLAGS := -std=gnu99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(if $(NATIVE_BITNESS_ONLY),-DNATIVE_BITNESS_ONLY) -D_DT_VERSION=\"$(VERSION)\"
CPPFLAGS += -Iinclude -Iuts/common -Iinclude/$(ARCHINC) -I$(objdir)
export CC = gcc
override CFLAGS += $(INVARIANT_CFLAGS)
PREPROCESS = $(CC) -E

# The first non-system uid on this system.
USER_UID=$(shell grep '^UID_MIN' /etc/login.defs | awk '{print $$2;}')

# The substitution process in libdtrace needs kernel build trees for every
# kernel this userspace will be used with.  It only needs to know about major
# versions because to a first approximation the kernel-header-file #defines and
# data structures needed in translators do not change on a finer grain than
# that.  It also needs to know the name of the kernel architecture (as used in
# pathnames), and about the pieces of the pathname before and after the kernel
# version (so it can build include paths).

KERNELS=$(shell uname -r)
KERNELDIRPREFIX=/lib/modules
KERNELODIR=
# This allows you to build using a locally installed kernel built with O= by
# just specifying KERNELODIR=relative/path/to/your/kernel/o/dir.
KERNELDIRSUFFIX=$(if $(KERNELODIR),/source,/build)
KERNELARCH := $(subst sparc64,sparc,$(subst x86_64,x86,$(shell uname -m)))

# If libdtrace-ctf is initialized, we want to get headers from it.

ifneq ($(wildcard libdtrace-ctf/Make*),)
CPPFLAGS += -Ilibdtrace-ctf/include
endif

# Paths.

prefix = /usr
export objdir := $(abspath build)
LIBDIR := $(prefix)/lib$(BITNESS)
INSTLIBDIR := $(DESTDIR)$(LIBDIR)
BINDIR := $(prefix)/bin
INSTBINDIR := $(DESTDIR)$(BINDIR)
INCLUDEDIR := $(prefix)/include
INSTINCLUDEDIR := $(DESTDIR)$(INCLUDEDIR)
SBINDIR := $(prefix)/sbin
INSTSBINDIR := $(DESTDIR)$(SBINDIR)
DOCDIR := $(prefix)/share/doc/dtrace-$(VERSION)
INSTDOCDIR := $(DESTDIR)$(DOCDIR)
MANDIR := $(prefix)/share/man/man1
INSTMANDIR := $(DESTDIR)$(MANDIR)
ETCDIR := /etc
INSTETCDIR := $(DESTDIR)$(ETCDIR)
TESTDIR := $(prefix)/lib$(BITNESS)/dtrace/testsuite
INSTTESTDIR := $(DESTDIR)$(TESTDIR)
TARGETS =

DTRACE ?= $(objdir)/dtrace

# Include everything.

all::

$(shell mkdir -p $(objdir))

include Makeoptions
include Makefunctions
include Makeconfig
include Build $(sort $(wildcard */Build))
-include $(objdir)/*.deps
include Makerules
include Maketargets
include Makecheck

# Tarball distribution.

PHONIES += dist

.git/index:

.git-version.tmp:  .git/index
	if [[ -f .git/index ]]; then \
		git log --no-walk --pretty=format:%H > .git-version.tmp; \
	else \
		cp .git-archive-version .git-version.tmp; \
	fi

.git-version: .git-version.tmp
	if test -r "$@" && cmp -s "$@" "$^"; then \
		rm -f "$^"; \
	else \
		printf "VERSION: .git-version\n"; \
		mv -f "$^" "$@"; \
	fi

dist::
	git archive --prefix=dtrace-utils-$(VERSION)/ HEAD | bzip2 > dtrace-utils-$(VERSION).tar.bz2

clean::
	rm -f .git-version

.PHONY: $(PHONIES)
