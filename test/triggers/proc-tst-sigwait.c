/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/dtrace_types.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int
main(int argc, char **argv)
{
	struct sigevent ev;
	struct itimerspec ts;
	sigset_t set;
	timer_t tid;
	char *cmd = argv[0];

	ev.sigev_notify = SIGEV_SIGNAL;
	ev.sigev_signo = SIGUSR1;

	if (timer_create(CLOCK_REALTIME, &ev, &tid) == -1) {
		fprintf(stderr, "%s: cannot create CLOCK_HIGHRES "
		    "timer: %s\n", cmd, strerror(errno));
		exit(EXIT_FAILURE);
	}

	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigprocmask(SIG_BLOCK, &set, NULL);

	ts.it_value.tv_sec = 5;
	ts.it_value.tv_nsec = 0;
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = NANOSEC / 2;

	if (timer_settime(tid, 0, &ts, NULL) == -1) {
		fprintf(stderr, "%s: timer_settime() failed: %s\n",
		    cmd, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (;;) {
		int sig;
		sigwait(&set, &sig);
	}

	/*NOTREACHED*/
	return 0;
}
