/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define	DTRACEIOC	(('d' << 24) | ('t' << 16) | ('r' << 8))
#define	DTRACEIOC_MAX	17

void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: ", "badioctl");
	vfprintf(stderr, fmt, ap);

	if (fmt[strlen(fmt) - 1] != '\n')
		fprintf(stderr, ": %s\n", strerror(errno));

	exit(1);
}

void
badioctl(pid_t parent)
{
	int fd = -1, random, ps = sysconf(_SC_PAGESIZE);
	int i = 0;
	caddr_t addr;
	struct timeval now, last = {0};

	if ((random = open("/dev/urandom", O_RDONLY)) == -1)
		fatal("couldn't open /dev/urandom");

	if ((addr = mmap(0, ps, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == (caddr_t)-1)
		fatal("mmap");

	for (;;) {
		unsigned int ioc;

		gettimeofday(&now, NULL);
		if (now.tv_sec > last.tv_sec) {
			if (kill(parent, 0) == -1 && errno == ESRCH) {
				/*
				 * Our parent died.  We will kill ourselves in
				 * sympathy.
				 */
				exit(0);
			}

			/*
			 * Once a second, we'll reopen the device.
			 */
			if (fd != -1)
				close(fd);

			fd = open("/dev/dtrace/dtrace", O_RDONLY);

			if (fd == -1)
				fatal("couldn't open DTrace pseudo device");

			last = now;
		}


		if ((i++ % 1000) == 0) {
			/*
			 * Every thousand iterations, change our random gunk.
			 */
			read(random, addr, ps);
		}

		read(random, &ioc, sizeof(ioc));
		ioc %= DTRACEIOC_MAX;
		ioc++;
		ioctl(fd, DTRACEIOC | ioc, addr);
	}
}

int
main()
{
	pid_t child, parent = getpid();
	int status;

	for (;;) {
		if ((child = fork()) == 0)
			badioctl(parent);

		while (waitpid(child, &status, WEXITED) != child)
			continue;

		if (WIFEXITED(status)) {
			/*
			 * Our child exited by design -- we'll exit with
			 * the same status code.
			 */
			exit(WEXITSTATUS(status));
		}

		/*
		 * Our child died on a signal.  Respawn it.
		 */
		printf("badioctl: child died on signal %d; respawning.\n",
		    WTERMSIG(status));
		fflush(stdout);
	}

	/* NOTREACHED */
	return 0;
}
