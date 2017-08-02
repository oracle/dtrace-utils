/*
 * Oracle Linux DTrace.
 * Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

int limit = 4096;

int grow1(int);

int
shouldGrow(int frame)
{
	return frame >= limit-- ? 0 : 1;
}

int
grow(int frame)
{
	/*
	 * Create a ridiculously large stack - enough to push us over
	 * the default setting of 'dtrace_ustackdepth_max' (2048).
	 *
	 * This loop used to repeatedly call getpid(), but on Linux the result
	 * of that call gets cached, so that repeated calls actually do not
	 * trigger a system call anymore.  We use ioctl() instead.
	 */
	if (shouldGrow(frame))
		frame = grow1(frame++);

	for (;;)
		ioctl(-1, -1, NULL);

	grow1(frame);
}

int
grow1(int frame)
{
	if (shouldGrow(frame))
		frame = grow(frame++);

	for (;;)
		ioctl(-2, -2, NULL);

	grow(frame);
}

int
main(int argc, char *argv[])
{
	grow(1);

	return (0);
}
