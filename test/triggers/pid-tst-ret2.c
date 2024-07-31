/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
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
go(void)
{
	return a;
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

	signal(SIGUSR1, handle);

	getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = 1024;
	setrlimit(RLIMIT_NOFILE, &rl);
	getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_cur; i++)
		close(i);

	for (;;) {
		ioctl(-1, -1, NULL);
		usleep(100);
	}
}
