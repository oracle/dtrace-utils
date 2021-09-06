/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A clause can have multiple commits and discards.
 *
 * SECTION: Speculative Tracing/Committing a Speculation
 *
 */
#pragma D option quiet
#pragma D option nspec=8

BEGIN
{
	a = speculation();
	b = speculation();
	c = speculation();
	d = speculation();
	e = speculation();
	f = speculation();
	g = speculation();
	h = speculation();
	printf("Speculation IDs: %d %d %d %d %d %d %d %d\n",
	       a, b, c, d, e, f, g, h);
}

BEGIN
{
	speculate(a);
	printf("Speculating on id: %d\n", a);
}

BEGIN
{
	speculate(b);
	printf("Speculating on id: %d\n", b);
}

BEGIN
{
	speculate(c);
	printf("Speculating on id: %d\n", c);
}

BEGIN
{
	speculate(d);
	printf("Speculating on id: %d\n", d);
}

BEGIN
{
	speculate(e);
	printf("Speculating on id: %d\n", e);
}

BEGIN
{
	speculate(f);
	printf("Speculating on id: %d\n", f);
}

BEGIN
{
	speculate(g);
	printf("Speculating on id: %d\n", g);
}

BEGIN
{
	speculate(h);
	printf("Speculating on id: %d\n", h);
}

BEGIN
{
	commit(h);
	discard(g);
	commit(f);
	discard(e);
	commit(d);
	discard(c);
	commit(b);
	discard(a);
}

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
