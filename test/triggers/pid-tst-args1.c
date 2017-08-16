/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

int
go(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6,
    int arg7, int arg8, int arg9)
{
	return (arg1);
}

static void
handle(int sig)
{
	go(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	exit(0);
}

int
main(int argc, char **argv)
{
	(void) signal(SIGUSR1, handle);
	for (;;)
		getpid();
}
