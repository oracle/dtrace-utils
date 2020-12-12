/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
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

	sigemptyset(&ss);
	sigaddset(&ss, SIGINT);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	do {
		ioctl(-1, -1, NULL);
		sigpending(&ss);
	} while (!sigismember(&ss, SIGINT));

	return 0;
}
