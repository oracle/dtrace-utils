/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>

int
waiting(volatile int *a)
{
	return (*a);
}

int
go(void)
{
	int i, j, total = 0;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 10; j++) {
			total += i * j;
		}
	}

	return (total);
}

int
main(int argc, char **argv)
{
	volatile int a = 0;

	while (waiting(&a) == 0)
		continue;

	if (vfork() == 0) {
		int ret = go();
		(void) _exit(ret);
	}

	return (0);
}
