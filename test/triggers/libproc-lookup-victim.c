/*
 * Symbol lookup testcase.
 *
 * This victim halts itself until the test runner has caught up, then executes
 * look up one of a variety of functions depending on our lone argument.  The
 * test runner drops a breakpoint on the same function and kills us afterwards,
 * so we scream if it is ever called.
 */

/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "libproc-lookup-victim.h"

void
main_func(void) {
	fprintf(stderr, "%s called: this should never be seen.\n", __func__);
}

void
interposed_func(void) {
	fprintf(stderr, "%s called: This should never be seen.\n", __func__);
}

int
main(int argc, char *argv[])
{
	void (*func) (void);
	void *dlmopen_handle;

	enum { MAIN, LIB, INTERPOSED, LMID } state;

	if (argc != 2) {
		fprintf(stderr, "Syntax: libproc-lookup-victim {0..3}.\n");
		exit(1);
	}

	state = strtol(argv[1], NULL, 10);

	switch (state) {
	case MAIN: func = main_func;
		break;
	case LIB: func = lib_func;
		break;
	case INTERPOSED:
		func = interpose_func;
		break;
	case LMID:
		dlmopen_handle = dlmopen(LM_ID_NEWLM,
		    "test/triggers/libproc-lookup-victim-lib.so", RTLD_NOW);
		if (!dlmopen_handle) {
			fprintf(stderr, "Cannot dlmopen(): %s\n", dlerror());
			exit(1);
		}
		func = dlsym(dlmopen_handle, "lib_func");
		break;
	default:
		fprintf(stderr, "Invalid argument %s\n", argv[1]);
		exit(1);
	}

	raise(SIGSTOP);
	func();
	fprintf(stderr, "Error: symbol resolution failed.\n");
	return 1;
}
