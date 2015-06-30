/*
 * Symbol lookup testcase.
 *
 * This victim halts itself until the test runner has caught up, then executes
 * look up one of a variety of functions depending on our lone argument.  The
 * test runner drops a breakpoint on the same function and kills us afterwards,
 * so we scream if it is ever called.
 */

/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "libproc-lookup-victim.h"

/*
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

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
