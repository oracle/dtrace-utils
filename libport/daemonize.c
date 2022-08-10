/*
 * Oracle Linux DTrace; become a daemon.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/compiler.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <port.h>

/*
 * Write an error return down the synchronization pipe.
 */
_dt_noreturn_
void
daemon_err(int fd, const char *err)
{
	/*
	 * Not a paranoid write, no EINTR protection: all our errors are quite
	 * short and are unlikely to hit EINTR.  The read side, which might
	 * block for some time,  can make no such assumptions.
	 */
	write(fd, err, strlen(err));
	_exit(1);
}

/*
 * Write an error return featuring errno down the synchronization pipe.
 *
 * If fd is < 0, write to stderr instead.
 */
_dt_noreturn_
void
daemon_perr(int fd, const char *err, int err_no)
{
	char sep[] = ": ";
	char *errstr;

	/*
	 * Not a paranoid write: see above.
	 */
	if (fd >= 0) {
		write(fd, err, strlen(err));
		if ((errstr = strerror(err_no)) != NULL) {
			write(fd, sep, strlen(sep));
			write(fd, errstr, strlen(errstr));
		}
	} else
		fprintf(stderr, "%s: %s\n", err, strerror(err_no));

	_exit(1);
}

/*
 * On failure, returns -1 in the parent.
 *
 * On success, returns the fd of the synchronization pipe in the child: write
 * down it to indicate an error that should prevent daemonization, or close it
 * to indicate success.  That error will then trigger a return in the parent.
 */

int
daemonize(int close_fds)
{
	size_t i;
	struct sigaction sa = { 0 };
	sigset_t mask;
	int initialized[2];

	if (close_fds)
		if (close_range(3, ~0U, 0) < 0)
			return -1;		/* errno is set for us.  */

	if (pipe(initialized) < 0)
		return -1;			/* errno is set for us. */
	/*
	 * Explicitly avoid checking for errors here -- not all signals can be
	 * reset, and we can't do anything if reset fails anyway.
	 */
	sa.sa_handler = SIG_DFL;
	for (i = 0; i < _NSIG; i++)
		(void) sigaction(i, &sa, NULL);

	sigemptyset(&mask);
	(void) sigprocmask(SIG_SETMASK, &mask, NULL);

	switch (fork()) {
	case -1: perror("cannot fork");
		return -1;
	case 0: break;
	default:
	{
		char errsync[1024];
		char *errsyncp = errsync;
		int count = 0;

		memset(errsync, 0, sizeof(errsync));
		close(initialized[1]);

		/*
		 * Wait for successful daemonization of the child.  If it
		 * fails, print the message passed back as an error.
		 *
		 * Be very paranoid here: we know almost nothing about the state
		 * of the child *or* ourselves.
		 */
		for (errno = 0; (sizeof(errsync) - (errsyncp - errsync) >= 0) &&
			 (count > 0 || (count < 0 && errno == EINTR));
			 errno = 0) {

			count = read(initialized[0], errsyncp,
			    sizeof(errsync) - (errsyncp - errsync));

			if (count > 0)
				errsyncp += count;
		}

		close(initialized[0]);

		/*
		 * Child successfully initialized: terminate parent.
		 */
		if (errno == 0 && errsyncp == errsync)
			_exit(0);

		if (errno != 0) {
			perror("cannot synchronize with daemon: state unknown");
			return -1;
		}

		fprintf(stderr, "%s\n", errsync);
		return -1;
	}
	}

	close(initialized[0]);
	(void) setsid();

	switch (fork()) {
	case -1: daemon_perr(initialized[1], "cannot fork", errno);
	case 0: break;
	default: _exit(0);
	}

	close(0);
	if (open("/dev/null", O_RDWR) != 0)
		daemon_perr(initialized[1], "cannot open /dev/null", errno);

	if (dup2(0, 1) < 0 ||
	    dup2(0, 2) < 0)
		daemon_perr(initialized[1], "cannot dup2 standard fds", errno);

	umask(0);
	if (chdir("/") < 0)
		daemon_perr(initialized[1], "cannot chdir to /", errno);

	return initialized[1];
}
