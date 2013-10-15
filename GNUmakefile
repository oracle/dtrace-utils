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
# Copyright 2011, 2012, 2013, 2014 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

.DELETE_ON_ERROR:
.SUFFIXES:
.SECONDEXPANSION:

PROJECT := dtrace
VERSION := 0.4.5

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
INVARIANT_CFLAGS := -std=gnu99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(if $(NATIVE_BITNESS_ONLY),-DNATIVE_BITNESS_ONLY)
CPPFLAGS += -Iinclude -Iuts/common -Iinclude/$(ARCHINC) -I$(objdir)
export CC = gcc
override CFLAGS += $(INVARIANT_CFLAGS)
PREPROCESS = $(CC) -E

# The substitution process in libdtrace needs a kernel build tree, and needs to
# know the name of the kernel architecture (as used in pathnames).

KERNELDIR := /lib/modules/$(shell uname -r)/build
KERNELARCH := x86

# If libdtrace-ctf is initialized, we want to get headers from it.

ifneq ($(wildcard libdtrace-ctf/Make*),)
CPPFLAGS += -Ilibdtrace-ctf/include
endif

prefix = /usr
export objdir := $(abspath build)
LIBDIR := $(DESTDIR)$(prefix)/lib$(BITNESS)
BINDIR := $(DESTDIR)$(prefix)/bin
INCLUDEDIR := $(DESTDIR)$(prefix)/include
SBINDIR := $(DESTDIR)$(prefix)/sbin
DOCDIR := $(DESTDIR)$(prefix)/share/doc/dtrace-$(VERSION)
MANDIR := $(DESTDIR)$(prefix)/share/man/man1
TARGETS =

DTRACE ?= $(objdir)/dtrace

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

.PHONY: $(PHONIES)
