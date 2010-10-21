BUILD_DIR=build-$(shell uname -r)
NOPWD = --no-print-directory
all:
	$(MAKE) all1
	cd libctf; $(MAKE) $(NOPWD)
	cd libdtrace ; $(MAKE) $(NOPWD)
	cd libport ; $(MAKE) $(NOPWD)
	cd libproc/common ; $(MAKE) $(NOPWD)
	cd librtld ; $(MAKE) $(NOPWD)
	cd cmd/common ; $(MAKE) $(NOPWD)
all1:
	if [ ! -d $(BUILD_DIR) ] ; then \
		mkdir $(BUILD_DIR); \
	fi
clean:
	-rm -rf $(BUILD_DIR)
