/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

static sigjmp_buf env;

static void
interrupt(int sig)
{
	siglongjmp(env, sig);
}

int
main(void)
{
	const char *file = "/proc/self/mem";
	int i, n, fds[10];
	struct sigaction act;
	struct rlimit rl;

	act.sa_handler = interrupt;
	act.sa_flags = 0;

	sigemptyset(&act.sa_mask);
	sigaction(SIGUSR1, &act, NULL);

	getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_max; i++)
		close(i);
	n = 0;

	/*
	 * With all of our file descriptors closed, wait here spinning in bogus
	 * ioctl() calls until DTrace hits us with a SIGUSR1 to start the test.
	 */
	if (sigsetjmp(env, 1) == 0)
		for (;;) {
			ioctl(-1, -1, NULL);
                        usleep(100);
                }

	/*
	 * To test the fds[] array, we open /dev/null (a file with reliable
	 * pathname and properties) using various flags and seek offsets.
	 */
	fds[n++] = open(file, O_RDONLY);
	fds[n++] = open(file, O_WRONLY);
	fds[n++] = open(file, O_RDWR);

	fds[n++] = open(file, O_RDWR | O_APPEND | O_CREAT | O_DSYNC |
	    O_LARGEFILE | O_NOCTTY | O_NONBLOCK | O_NDELAY | O_RSYNC |
	    O_SYNC | O_TRUNC, 0);

	fds[n++] = open(file, O_RDWR);
	lseek(fds[n - 1], 123, SEEK_SET);

	/*
	 * Once we have all the file descriptors in the state we want to test,
	 * issue a bogus ioctl() on each fd with cmd -1 and arg NULL to whack
	 * our DTrace script into recording the content of the fds[] array.
	 */
	for (i = 0; i < n; i++)
		ioctl(fds[i], -1, NULL);

	assert(n <= sizeof(fds) / sizeof(fds[0]));
	exit(0);
}
