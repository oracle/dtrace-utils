BUILD_DIR := ../$(BUILD_DIR)
SBINDIR = /usr/sbin
CPPFLAGS += $(DTO) -D_LONGLONG_TYPE

LIBELF=/usr/lib/libelf.so.1
LIBS = \
        $(BUILD_DIR)/libctf.a \
        $(BUILD_DIR)/libdtrace.a \
        $(BUILD_DIR)/libproc.a \
        $(BUILD_DIR)/libport.a

COMPILE = $(CC) $(CFLAGS) -o $(BUILD_DIR)/dtrace \
	-L$(BUILD_DIR) $$LIB32 \
	$(BUILD_DIR)/dtrace.o -ldtrace -lctf -lproc -lport -lz -lrt -lpthread $$libelf -ldl

$(BUILD_DIR)/dtrace: $(BUILD_DIR)/dtrace.o $(LIBS)
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

$(BUILD_DIR)/dtrace.o: dtrace.c
	$(CC) $(CPPFLAGS) -c \
	-I../uts/common \
	-I../libctf \
	-I../libdtrace \
	-I../libproc/common \
	-I../include \
	dtrace.c
	mv dtrace.o $(BUILD_DIR)

install: $(BUILD_DIR)/dtrace
	mkdir -p $(DESTDIR)$(SBINDIR)
	install -m 755 -o root -g root $(BUILD_DIR)/dtrace $(DESTDIR)$(SBINDIR)
