/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * The canonical name should be 'go' since we prefer symbol names with fewer
 * leading underscores.
 */

int a = 100;

int
go(void)
{
	return (a);
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
	(void) signal(SIGUSR1, handle);
	for (;;)
		getpid();
}
