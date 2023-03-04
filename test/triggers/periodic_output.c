/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* emit periodic output */

#include <stdio.h>
#include <time.h>

int main (void)
{
	struct timespec t;
	int i = 0;
	FILE *fp = fopen("/dev/null", "a");

	t.tv_sec = 1;
	t.tv_nsec = 0;

	while (1) {
		fprintf(fp, "periodic output %4.4d\n", i++);
		fflush(fp);
		nanosleep(&t, NULL);
	}

	return 0;
}
