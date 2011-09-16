.DELETE_ON_ERROR:
.SUFFIXES:

# Verify supported hardware.

$(if $(subst "x86_64",,$(shell uname -m)),,$(error "Error: Dtrace for Linux only supports x86_64"),)
$(if $(subst "Linux",,$(shell uname -s)),,$(error "Error: Dtrace only supports Linux"),)

CFLAGS ?= -O2 -g -Wall -pedantic -Wno-unknown-pragmas
LDFLAGS ?=
INVARIANT_CFLAGS := -std=gnu99 -D_LITTLE_ENDIAN -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(DTO) -D_ILP$(BITNESS) -DCTF_OLD_VERSIONS
BITNESS := 64
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
TARGETS =

all::

$(shell mkdir -p $(objdir))

include Makeoptions
include Makefunctions
include */Build
-include $(objdir)/*.d
include Makerules

all:: $(TARGETS)

clean::
	-rm -rf $(objdir) test/log

check: all
	./runtest.sh --quiet

check-verbose: all
	./runtest.sh --verbose

.PHONY: $(PHONIES)
