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

/*
 * The canonical name should be 'go' since we prefer symbol names with fewer
 * leading underscores.
 */

int a = 100;

int
help(void)
{
	return (a);
}

int
go(void)
{
	return (help() + 1);
}

static void
handle(int sig)
{
	go();
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
