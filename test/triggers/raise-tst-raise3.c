/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>

static void
handle(int sig)
{
	exit(0);
}

int
main(int argc, char **argv)
{
	struct sigaction sa;

	sa.sa_handler = handle;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	(void) sigaction(SIGINT, &sa, NULL);

	for (;;) {
		(void) ioctl(-1, -1, NULL);
	}
}
