CC=/usr/bin/gcc -g
PTR32=-D_ILP32
CPPFLAGS += -I../libproc/common \
        -I../common/ctf \
        -I../include \
        -I../libctf \
        -I../uts/common \
        -I. \
        -DCTF_OLD_VERSIONS $(PTR32)
CPPFLAGS += -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE

BINDIR=../build-2.6.18-128.1.14.0.1.el5/

LIB = $(BINDIR)/libdtrace.a

all: $(LIB)
	@/bin/true

$(LIB): \
	$(LIB)(rtld_db.o)

clean:
	-rm -f *.o *.a

