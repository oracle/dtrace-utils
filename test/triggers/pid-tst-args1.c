/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

typedef long int	l_int;

int
go(l_int arg0, l_int arg1, l_int arg2, l_int arg3, l_int arg4,
   l_int arg5, l_int arg6, l_int arg7, l_int arg8, l_int arg9)
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
	int		i;
	struct rlimit	rl;

	(void) signal(SIGUSR1, handle);

	getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_max; i++)
		close(i);

	for (;;) {
		ioctl(-1, -1, NULL);
		usleep(100);
	}
}
