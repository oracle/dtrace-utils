/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <spawn.h>

void
go(void)
{
	char *argv[] = { "/bin/ls", NULL };
	pid_t pid;
	int null = open("/dev/null", O_RDWR);

	/*
	 * Don't spam the screen with ls output.
	 */
	dup2(null, 0);
	dup2(null, 1);
	dup2(null, 2);
	close(null);

	posix_spawn(&pid, "/bin/ls", NULL, NULL, argv, NULL);

	waitpid(pid, NULL, 0);
}

void
intr(int sig)
{
}

int
main(int argc, char **argv)
{
	struct sigaction sa;

	sa.sa_handler = intr;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGUSR1, &sa, NULL);

	for (;;)
		go();

	return 0;
}
