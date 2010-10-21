CC=/usr/bin/gcc -g
BUILD_DIR=build-$(shell uname -r)
CPPFLAGS += -I../include
BINDIR = ../build-2.6.18-128.1.14.0.1.el5
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
