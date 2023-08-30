/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Loading from struct members works for alloca pointers.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

struct foo {
	int	a;
	char	b;
	int	c;
} *ptr;

BEGIN
{
	ptr = alloca(sizeof(struct foo));
	ptr->c = 0x42;
	trace(ptr->c);
	exit(0);
}

ERROR
{
	exit(1);
}
