/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

static sigjmp_buf env;

static void interrupt(int sig)
{
	siglongjmp(env, sig);
}

int go(int a)
{
	int i, j, total = 0;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 10; j++) {
			total += i * j + a;
		}
	}

	return (total);
}

int main(int argc, char **argv)
{
	int i;
	struct sigaction act;
	struct rlimit rl;

	act.sa_handler = interrupt;
	act.sa_flags = 0;

	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR1, &act, NULL);

	getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_max; i++)
		close(i);

	/* Wait until we get hit with a SIGUSR1 signal. */
	if (sigsetjmp(env, 1) == 0) {
		for (;;) {
			ioctl(-1, -1, NULL);
			usleep(100);
		}
	}

	(void) fork();
	(void) go(i);

	return (0);
}
