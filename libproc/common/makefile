CPPFLAGS += -I../common \
	-I../../common/ctf \
	-I../../uts/common/ \
	-I../../include \
	-I../../libproc/common \
	-I../../libctf \
	-I../../uts/intel/ \
	-DCTF_OLD_VERSIONS $(PTR32)
CPPFLAGS += $(DTO) -D_LONGLONG_TYPE 

LIB = ../../$(BUILD_DIR)/libproc.a

all: $(LIB)
	@/bin/true

$(LIB): \
	$(LIB)(Pcontrol.o) \
	$(LIB)(../amd64/Pisadep.o) \
	$(LIB)(Psymtab_machelf32.o) \
	$(LIB)(Psymtab_machelf64.o) \
	$(LIB)(Psymtab.o) \
	$(LIB)(proc.o) \
	$(LIB)(Putil.o)

clean:
	-rm -f *.o ../../$(BUILD_DIR)/libproc.a

