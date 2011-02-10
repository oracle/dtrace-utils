BUILD_DIR=build-$(shell uname -r)
NOPWD = --no-print-directory
CC=/usr/bin/gcc
COMPILE.cpp= $(CC) -E -C $(CFLAGS) $(CPPFLAGS)
all:
	$(MAKE) all1
	cd libctf; $(MAKE) $(NOPWD)
	cd libdtrace ; $(MAKE) $(NOPWD) all
	cd libport ; $(MAKE) $(NOPWD)
	cd libproc/common ; $(MAKE) $(NOPWD)
	cd librtld ; $(MAKE) $(NOPWD)
	cd cmd/common ; $(MAKE) $(NOPWD)
all1:
	if [ ! -d $(BUILD_DIR) ] ; then \
		mkdir $(BUILD_DIR); \
	fi
install:
	cd libdtrace ; $(MAKE) $(NOPWD) install
uninstall:
	cd libdtrace ; $(MAKE) $(NOPWD) uninstall
clean:
	-rm -rf $(BUILD_DIR)
	cd libctf; $(MAKE) $(NOPWD) clean
	cd libdtrace ; $(MAKE) $(NOPWD) clean
	cd libport ; $(MAKE) $(NOPWD) clean
	cd libproc/common ; $(MAKE) $(NOPWD) clean 
	cd librtld ; $(MAKE) $(NOPWD) clean
