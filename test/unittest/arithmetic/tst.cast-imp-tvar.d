/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Integers are typecast correctly (implicit)
 *	       when using thread-local variables.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 */
/* @@runtest-opts: -qC -DUSE_AS_D_SCRIPT */

self signed char c, c0;
self short s, s0;
self int i, i0;
self long long l, l0;
self unsigned char C, C0;
self unsigned short S, S0;
self unsigned int I, I0;
self unsigned long long L, L0;

#define FMT "%d %d %d %d %d %d %d %d\n"

#define TEST(x) \
	self->c = (x); self->s = (x); self->i = (x); self->l = (x); \
	self->C = (x); self->S = (x); self->I = (x); self->L = (x); \
	printf(FMT, self->c, self->s, self->i, self->l, self->C, self->S, self->I, self->L)

BEGIN
{
	/* from scalar */
	TEST(-2);
	TEST(0xfffffffffffffffe);
	TEST(0xfffffffe);
	TEST(0xfffe);
	TEST(0xfe);
	TEST(2);
	TEST(0x55);
	TEST(0x5555);
	TEST(0x55555555);
	TEST(0x5555555555555555);

	/* from signed char */
	self->c0 = -2; TEST(self->c0);
	self->c0 = 0xfe; TEST(self->c0);
	self->c0 = 2; TEST(self->c0);
	self->c0 = 0x55; TEST(self->c0);

	/* from short */
	self->s0 = -2; TEST(self->s0);
	self->s0 = 0xfffe; TEST(self->s0);
	self->s0 = 2; TEST(self->s0);
	self->s0 = 0x5555; TEST(self->s0);

	/* from int */
	self->i0 = -2; TEST(self->i0);
	self->i0 = 0xfffffffe; TEST(self->i0);
	self->i0 = 2; TEST(self->i0);
	self->i0 = 0x55555555; TEST(self->i0);

	/* from long long */
	self->l0 = -2; TEST(self->l0);
	self->l0 = 0xfffffffffffffffe; TEST(self->l0);
	self->l0 = 2; TEST(self->l0);
	self->l0 = 0x5555555555555555; TEST(self->l0);

	/* from unsigned char */
	self->C0 = -2; TEST(self->C0);
	self->C0 = 0xfe; TEST(self->C0);
	self->C0 = 2; TEST(self->C0);
	self->C0 = 0x55; TEST(self->C0);

	/* from unsigned short */
	self->S0 = -2; TEST(self->S0);
	self->S0 = 0xfffe; TEST(self->S0);
	self->S0 = 2; TEST(self->S0);
	self->S0 = 0x5555; TEST(self->S0);

	/* from unsigned int */
	self->I0 = -2; TEST(self->I0);
	self->I0 = 0xfffffffe; TEST(self->I0);
	self->I0 = 2; TEST(self->I0);
	self->I0 = 0x55555555; TEST(self->I0);

	/* from unsigned long long */
	self->L0 = -2; TEST(self->L0);
	self->L0 = 0xfffffffffffffffe; TEST(self->L0);
	self->L0 = 2; TEST(self->L0);
	self->L0 = 0x5555555555555555; TEST(self->L0);

	exit (0);
}
