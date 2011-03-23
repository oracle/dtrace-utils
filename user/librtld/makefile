CPPFLAGS += -I../libproc/common \
        -I../common/ctf \
        -I../include \
        -I../libctf \
        -I../uts/common \
        -I. \
        -DCTF_OLD_VERSIONS $(DTO)
CPPFLAGS += -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE

BINDIR=../$(BUILD_DIR)

LIB = $(BINDIR)/libdtrace.a

all: $(LIB)
	@/bin/true

$(LIB): \
	$(LIB)(rd_elf.o) \
	$(LIB)(rtld_db.o)
clean:
	-rm -f *.o *.a

