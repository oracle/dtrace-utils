CPPFLAGS += -I../common \
	-I../../common/ctf \
	-I../../uts/common/ \
	-I../../include \
	-I../../libproc/common \
	-I../../libctf \
	-I../../uts/intel/ \
	-DCTF_OLD_VERSIONS $(PTR32)
CPPFLAGS += $(DTO) -D_LONGLONG_TYPE -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64

LIB = ../../$(BUILD_DIR)/libproc.a

all: $(LIB)
	@/bin/true

$(LIB): \
	$(LIB)(P32ton.o) \
	$(LIB)(Pcontrol.o) \
	$(LIB)(Pcore.o) \
	$(LIB)(Pexecname.o) \
	$(LIB)(Pidle.o) \
	$(LIB)(../amd64/Pisadep.o) \
	$(LIB)(Psymtab_machelf32.o) \
	$(LIB)(Psymtab_machelf64.o) \
	$(LIB)(Pscantext.o) \
	$(LIB)(Pservice.o) \
	$(LIB)(Pstack.o) \
	$(LIB)(Psymtab.o) \
	$(LIB)(Psyscall.o) \
	$(LIB)(Putil.o) \
	$(LIB)(Pzone.o)	\

$(LIB)(Pcore.o): Pcore.c
	$(CC) $(CPPFLAGS) -D_LARGEFILE64_SOURCE=1 -D_XOPEN_SOURCE=500 -c Pcore.c
	ar rv $(LIB) Pcore.o
	rm -rf Pcore.o

clean:
	-rm -f *.o ../../$(BUILD_DIR)/libproc.a
	
