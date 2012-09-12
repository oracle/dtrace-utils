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

PROJECT := dtrace
VERSION := trunk

# Verify supported hardware.

$(if $(subst "x86_64",,$(shell uname -m)),,$(error "Error: DTrace for Linux only supports x86_64"),)
$(if $(subst "Linux",,$(shell uname -s)),,$(error "Error: DTrace only supports Linux"),)

CFLAGS ?= -O2 -g -Wall -pedantic -Wno-unknown-pragmas
LDFLAGS ?=
BITNESS := 64
INVARIANT_CFLAGS := -std=gnu99 -D_LITTLE_ENDIAN -D_GNU_SOURCE $(DTO) -D_ILP$(BITNESS)
CPPFLAGS += -Iinclude -Iuts/common -I$(objdir)
CC = gcc
override CFLAGS += $(INVARIANT_CFLAGS)
PREPROCESS = $(CC) -E -C

# The substitution process in libdtrace needs a kernel build tree.

KERNELDIR := /lib/modules/$(shell uname -r)/build
ifeq ($(wildcard $(KERNELDIR)/include/linux),)
$(error "Error: Point KERNELDIR=... at the Linux kernel source")
endif

# If libdtrace-ctf is initialized, we want to get headers from it.

ifneq ($(wildcard libdtrace-ctf/Make*),)
CPPFLAGS += -Ilibdtrace-ctf/include
endif

prefix = /usr
objdir := build
LIBDIR := $(DESTDIR)$(prefix)/lib$(BITNESS)
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
include Makeconfig
include Build */Build
-include $(objdir)/*.d
include Makerules
include Makecheck

all:: $(TARGETS)

clean::
	$(call describe-target,CLEAN,$(objdir) test/log)
	-rm -rf $(objdir) test/log

realclean: clean
	-rm -f TAGS tags GTAGS GRTAGS GPATH

TAGS:
	$(call describe-target,TAGS)
	rm -f TAGS; find . -name '*.[ch]' | xargs etags -a

tags:
	$(call describe-target,tags)
	rm -f tags; find . -name '*.[ch]' | xargs ctags -a

gtags:
	$(call describe-target,gtags)
	gtags -i

PHONIES += all clean TAGS tags gtags

.PHONY: $(PHONIES)
