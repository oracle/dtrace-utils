CC=gcc -g
BUILD_DIR=build-$(shell uname -r)
CPPFLAGS += -I../include
BINDIR = ../$(BUILD_DIR)
LIB = $(BINDIR)/libport.a

all: $(LIB)
	@/bin/true

$(LIB): \
	$(LIB)(tasksys.o) \
	$(LIB)(gmatch.o) \
	$(LIB)(proc_name.o) \
	$(LIB)(strlcat.o) \
	$(LIB)(strlcpy.o) \
	$(LIB)(p_online.o) \
	$(LIB)(time.o)

clean:
	-rm -f *.o $(LIB)
