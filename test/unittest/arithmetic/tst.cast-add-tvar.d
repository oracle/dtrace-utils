/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Integers are promoted correctly for mixed-type adds
 *	       when using thread-local variables.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 */
/* @@runtest-opts: -qC */

self signed char c;
self short s;
self int i;
self long long l;
self unsigned char C;
self unsigned short S;
self unsigned int I;
self unsigned long long L;

#define FMT " %d"
#define USE_FMT(f)

#define TEST(x, y) \
  self->x = -2; self->y = -3; printf(FMT, self->x + self->y); printf(FMT, self->y + self->x); \
  self->x = -2; self->y = +3; printf(FMT, self->x + self->y); printf(FMT, self->y + self->x); \
  self->x = +2; self->y = -3; printf(FMT, self->x + self->y); printf(FMT, self->y + self->x); \
  self->x = +2; self->y = +3; printf(FMT, self->x + self->y); printf(FMT, self->y + self->x); printf("\n");

BEGIN
{
	/* cast to signed char */
	USE_FMT(" %hhd");
	TEST(c, c)

	/* cast to unsigned char */
	USE_FMT(" %hhu");
	TEST(C, c)
	TEST(C, C)

	/* cast to short */
	USE_FMT(" %hd");
	TEST(s, c)
	TEST(s, C)
	TEST(s, s)

	/* cast to unsigned short */
	USE_FMT(" %hu");
	TEST(S, c)
	TEST(S, C)
	TEST(S, s)
	TEST(S, S)

	/* cast to int */
	USE_FMT(" %d");
	TEST(i, c)
	TEST(i, C)
	TEST(i, s)
	TEST(i, S)
	TEST(i, i)

	/* cast to unsigned int */
	USE_FMT(" %u");
	TEST(I, c)
	TEST(I, C)
	TEST(I, s)
	TEST(I, S)
	TEST(I, i)
	TEST(I, I)

	/* cast to long long */
	USE_FMT(" %lld");
	TEST(l, c)
	TEST(l, C)
	TEST(l, s)
	TEST(l, S)
	TEST(l, i)
	TEST(l, I)
	TEST(l, l)

	/* cast to unsigned long long */
	USE_FMT(" %llu");
	TEST(L, c)
	TEST(L, C)
	TEST(L, s)
	TEST(L, S)
	TEST(L, i)
	TEST(L, I)
	TEST(L, l)
	TEST(L, L)

	exit (0);
}
