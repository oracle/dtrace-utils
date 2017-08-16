/*
 * Oracle Linux DTrace.
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Exit after a short delay.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main (void)
{
	struct timespec t;
	char *delay;

	delay = getenv("delay");
	if (!delay) {
		fprintf(stderr, "Delay in ns needed in delay env var.\n");
		exit(1);
	}

	t.tv_sec = strtoll(delay, NULL, 10) / 1000000000;
	t.tv_nsec = strtoll(delay, NULL, 10) % 1000000000;
	errno = 0;
	do {
		if (nanosleep(&t, &t) == 0)
			exit(0);
	} while (errno == -EINTR);
	exit(0);
}
