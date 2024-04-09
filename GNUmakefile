# Top-level makefile for dtrace.
#
# Build files in subdirectories are included by this file.
#
# Oracle Linux DTrace.
# Copyright (c) 2011, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

.DELETE_ON_ERROR:
.SUFFIXES:
.SECONDEXPANSION:

SHELL = /bin/bash

PROJECT := dtrace
VERSION := 2.0.0

# Verify supported hardware.

ARCH := $(shell uname -m)

$(if $(subst sparc64,,$(subst aarch64,,$(subst x86_64,,$(ARCH)))), \
    $(error "Error: DTrace for Linux only supports x86_64, ARM64 and sparc64"),)
$(if $(subst Linux,,$(shell uname -s)), \
    $(error "Error: DTrace only supports Linux"),)

# Variables overridable by the command line and configure scripts.

CFLAGS ?= -O2 -Wall -pedantic -Wno-unknown-pragmas
LDFLAGS ?=

# The first non-system uid on this system.
USER_UID := $(shell grep '^UID_MIN' /etc/login.defs | awk '{print $$2;}')

# A uid suitable for unprivileged execution.
UNPRIV_UID ?= -3

# The group one must run as to invoke dumpcap: by default the group of
# the dumpcap binary.  If dumpcap is owned by root, use the same gid as
# the UNPRIV_UID unless otherwise overridden.
# (We avoid ?= here to avoid rerunning the stat over and over again.)
ifeq ($(origin DUMPCAP_GROUP), undefined)
DUMPCAP_GROUP := $(filter-out root,$(shell stat -c %G /usr/sbin/dumpcap /usr/bin/dumpcap 2>/dev/null | head -1))
endif

# Unwritable but readable directory suitable for overriding as the $HOME of
# unprivileged processes.
UNPRIV_HOME ?= /run/initramfs

# The substitution process in libdtrace needs kernel build trees for every
# kernel this userspace will be used with.  It only needs to know about major
# versions because to a first approximation the kernel-header-file #defines and
# data structures needed in translators do not change on a finer grain than
# that.  It also needs to know the name of the kernel architecture (as used in
# pathnames), and about the pieces of the pathname before and after the kernel
# version (so it can build include paths).

# Headers for installed kernels should be found automatically by the build
# logic.  For local builds, one should specify KERNELSRCDIR as the source
# location of the kernel tree, and KERNELOBJDIR as the location of the object
# tree.  If the kernel was built within the source tree, KERNELOBJDIR need not
# be specified.

# For RPM builds, set KERNELMODDIR to the root of the rpmsrc tree, and set
# KERNELSRCNAME and KERNELBLDNAME to the empty string.

KERNELS ?= $(shell uname -r)
KERNELMODDIR ?= /lib/modules
KERNELSRCNAME ?= source
KERNELBLDNAME ?= build

ifdef KERNELSRCDIR
KERNELOBJDIR ?= $(KERNELSRCDIR)
endif

KERNELARCH := $(subst sparc64,sparc,$(subst aarch64,arm64,$(subst x86_64,x86,$(ARCH))))

# Paths.

prefix ?= /usr
export objdir := $(abspath build)
LIBDIR = $(prefix)/lib$(BITNESS)
INSTLIBDIR = $(DESTDIR)$(LIBDIR)
BINDIR = $(prefix)/bin
INSTBINDIR = $(DESTDIR)$(BINDIR)
INCLUDEDIR = $(prefix)/include
INSTINCLUDEDIR = $(DESTDIR)$(INCLUDEDIR)
SBINDIR = $(prefix)/sbin
INSTSBINDIR = $(DESTDIR)$(SBINDIR)
UDEVDIR = $(prefix)/lib/udev/rules.d
INSTUDEVDIR = $(DESTDIR)$(UDEVDIR)
SYSTEMDPRESETDIR = $(prefix)/lib/systemd/system-preset
SYSTEMDUNITDIR = $(prefix)/lib/systemd/system
INSTSYSTEMDPRESETDIR = $(DESTDIR)$(SYSTEMDPRESETDIR)
INSTSYSTEMDUNITDIR = $(DESTDIR)$(SYSTEMDUNITDIR)
DOCDIR = $(prefix)/share/doc/dtrace-$(VERSION)
INSTDOCDIR = $(DESTDIR)$(DOCDIR)
MANDIR = $(prefix)/share/man/man8
INSTMANDIR = $(DESTDIR)$(MANDIR)
TESTDIR = $(prefix)/lib$(BITNESS)/dtrace/testsuite
INSTTESTDIR = $(DESTDIR)$(TESTDIR)
TARGETS =

DTRACE ?= $(objdir)/dtrace

# configure overrides (themselves overridden by explicit command-line options).

-include $(objdir)/config-vars.mk

# Variables derived from the above.

export CC ?= gcc

BITNESS := 64
NATIVE_BITNESS_ONLY := $(shell echo 'int main (void) { }' | $(CC) -x c -o /dev/null -m32 - 2>/dev/null || echo t)
ARCHINC := $(subst sparc64,sparc,$(subst aarch64,arm64,$(subst x86_64,i386,$(ARCH))))

INVARIANT_CFLAGS := -std=gnu99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(if $(NATIVE_BITNESS_ONLY),-DNATIVE_BITNESS_ONLY) -D_DT_VERSION=\"$(VERSION)\"
CPPFLAGS += -Iinclude -Iuts/common -Iinclude/$(ARCHINC) -I$(objdir)

override CFLAGS += $(INVARIANT_CFLAGS)
PREPROCESS = $(CC) -E
export BPFC ?= bpf-unknown-none-gcc

BPFCPPFLAGS += -D$(subst sparc64,__sparc,$(subst aarch64,__aarch64__,$(subst x86_64,__amd64,$(ARCH))))
BPFCFLAGS ?= -O2 -Wall -Wno-unknown-pragmas $(if $(HAVE_BPFV3),-mcpu=v3)
export BPFLD ?= bpf-unknown-none-ld

all::

# Include everything.

$(shell mkdir -p $(objdir))

include Makeoptions
include Makefunctions
include Makeconfig

# Building config.mk is quite expensive: avoid doing it when only
# documentation targets and such things are invoked.
ifeq ($(strip $(filter %clean help% dist tags TAGS gtags,$(MAKECMDGOALS))),)
-include $(objdir)/config.mk
endif
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
	git archive --prefix=dtrace-$(VERSION)/ HEAD | bzip2 > dtrace-$(VERSION).tar.bz2

clean::
	rm -f .git-version

.PHONY: $(PHONIES)
