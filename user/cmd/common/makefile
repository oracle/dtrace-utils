CC=gcc -g $(BUILD_BITS)
BUILD_DIR=build-$(shell uname -r)
BINDIR=../../$(BUILD_DIR)
CPPFLAGS += -D_ILP32 -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -D_LONGLONG_TYPE

LIBELF=/usr/lib/libelf.so.1
LIBS = \
        $(BINDIR)/libctf.a \
        $(BINDIR)/libdtrace.a \
        $(BINDIR)/libproc.a \
        $(BINDIR)/libport.a

COMPILE = $(CC) $(CFLAGS) -o $(BINDIR)/dtrace \
	-L$(BINDIR) $$LIB32 \
	$(BINDIR)/dtrace.o -ldtrace -lctf -lproc -lport -lz -lrt -lpthread $$libelf -ldl

$(BINDIR)/dtrace: $(BINDIR)/dtrace.o $(LIBS)
	@libelf=$(LIBELF) ; \
	if [ "x$(BUILD_BITS)" = "x-m32" ]; then \
	        LIB32=-L/usr/lib32 ; \
	fi ; \
	if [ -f /usr/lib64/libelf.so.1 ]; then \
                libelf=/usr/lib64/libelf.so.1 ; \
        elif [ -f /usr/local/lib/libelf.a ]; then \
                libelf=/usr/local/lib/libelf.a ; \
        fi ; \
        echo $(COMPILE) ; \
        $(COMPILE)

$(BINDIR)/dtrace.o: dtrace.c
	$(CC) $(CPPFLAGS) -c \
	-I../../uts/common \
	-I../../libctf \
	-I../../libdtrace \
	-I../../libproc/common \
	-I../../include \
	dtrace.c
	mv dtrace.o $(BINDIR)


