/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>

volatile long long count = 0;

int
baz(int a)
{
	(void) getpid();
	while (count != -1) {
		count++;
		a++;
	}

	return (a + 1);
}

int
bar(int a)
{
	return (baz(a + 1) - 1);
}

int
foo(int a, long b)
{
	return (bar(a) - b);
}

int
main(int argc, char **argv)
{
	/* bleah. */
	return (foo(argc, (long)argv) == 0);
}
