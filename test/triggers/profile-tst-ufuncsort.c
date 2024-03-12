/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <unistd.h>

typedef void f(int x);

static void
f_a(int i)
{
}

static void
f_b(int i)
{
}

static void
f_c(int i)
{
}

static void
f_d(int i)
{
}

static void
f_e(int i)
{
}

static void
fN(f func, int i)
{
	func(i);
}

int
main()
{
	fN(f_a, 1);
	fN(f_b, 2);
	fN(f_c, 3);
	fN(f_d, 4);
	fN(f_e, 5);
	fN(f_a, 11);
	fN(f_c, 13);
	fN(f_d, 14);
	fN(f_a, 101);
	fN(f_c, 103);
	fN(f_c, 1003);

	/* Add a long sleep, so the DTrace program can still convert user
	 * addresses to symbols.  An excessively long sleep is okay since DTrace
	 * can kill the target when it's done.
	 */
	usleep(100 * 1000 * 1000);

	return 0;
}
