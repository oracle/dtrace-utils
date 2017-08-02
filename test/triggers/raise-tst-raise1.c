/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

int
main(int argc, char **argv)
{
	sigset_t ss;

	(void) sigemptyset(&ss);
	(void) sigaddset(&ss, SIGINT);
	(void) sigprocmask(SIG_BLOCK, &ss, NULL);

	do {
		(void) ioctl(-1, -1, NULL);
		(void) sigpending(&ss);
	} while (!sigismember(&ss, SIGINT));

	return (0);
}
