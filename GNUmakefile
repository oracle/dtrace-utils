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
# Copyright 2011, 2012 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

.DELETE_ON_ERROR:
.SUFFIXES:

VERSION := trunk

# Verify supported hardware.

$(if $(subst "x86_64",,$(shell uname -m)),,$(error "Error: Dtrace for Linux only supports x86_64"),)
$(if $(subst "Linux",,$(shell uname -s)),,$(error "Error: Dtrace only supports Linux"),)

CFLAGS ?= -O2 -g -Wall -pedantic -Wno-unknown-pragmas
LDFLAGS ?=
BITNESS := 64
INVARIANT_CFLAGS := -std=gnu99 -D_LITTLE_ENDIAN -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(DTO) -D_ILP$(BITNESS) -DCTF_OLD_VERSIONS
CPPFLAGS += -Iinclude -Iuts/common
CC = gcc
override CFLAGS += $(INVARIANT_CFLAGS)
PREPROCESS = $(CC) -E -C

prefix = /usr
objdir := build-$(shell uname -r)
LIBDIR := $(DESTDIR)$(prefix)/lib
BINDIR := $(DESTDIR)$(prefix)/bin
INCLUDEDIR := $(DESTDIR)$(prefix)/include
SBINDIR := $(DESTDIR)$(prefix)/sbin
DOCDIR := $(DESTDIR)$(prefix)/share/doc/dtrace-$(VERSION)
MANDIR := $(DESTDIR)$(prefix)/share/man/man1
TARGETS =

all::

$(shell mkdir -p $(objdir))

include Makeoptions
include Makefunctions
include Build */Build
-include $(objdir)/*.d
include Makerules

all:: $(TARGETS)

clean::
	-rm -rf $(objdir) test/log

check: check-verbose
check: RUNTESTFLAGS+=--quiet

check-verbose: all
	./runtest.sh $(RUNTESTFLAGS)

check-installed: check-installed-verbose
check-installed: RUNTESTFLAGS+=--quiet

check-installed-verbose: triggers
	./runtest.sh --use-installed $(RUNTESTFLAGS)

check-stress: check
check-stress: RUNTESTFLAGS+=--testsuites=unittest,demo --no-comparison

check-verbose-stress: check-verbose
check-verbose-stress: RUNTESTFLAGS+=--testsuites=unittest,demo --no-comparison

check-installed-stress: check-installed
check-installed-stress: RUNTESTFLAGS+=--testsuites=unittest,demo --no-comparison

check-installed-verbose-stress: check-installed-verbose
check-installed-verbose-stress: RUNTESTFLAGS+=--testsuites=unittest,demo --no-comparison

TAGS:
	rm -f TAGS; find . -name '*.[ch]' | xargs etags -a

tags:
	rm -f TAGS; find . -name '*.[ch]' | xargs ctags -a

PHONIES += all clean check check-verbose check-installed check-installed-verbose TAGS tags

.PHONY: $(PHONIES)
