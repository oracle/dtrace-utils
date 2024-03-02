/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Integers are typecast correctly (explicit)
 *	       when using associative arrays.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 */
/* @@runtest-opts: -qC */
/* @@timeout: 120 */

signed char c[int], c0[int];
short s[int], s0[int];
int i[int], i0[int];
long long l[int], l0[int];
unsigned char C[int], C0[int];
unsigned short S[int], S0[int];
unsigned int I[int], I0[int];
unsigned long long L[int], L0[int];

#define FMT "%d %d %d %d %d %d %d %d\n"

#define TEST(x) \
	c[0] = (signed char)(x); \
	s[0] = (short)(x); \
	i[0] = (int)(x); \
	l[0] = (long long)(x); \
	C[0] = (unsigned char)(x); \
	S[0] = (unsigned short)(x); \
	I[0] = (unsigned int)(x); \
	L[0] = (unsigned long long)(x); \
	printf(FMT, c[0], s[0], i[0], l[0], C[0], S[0], I[0], L[0])

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
	c0[0] = -2; TEST(c0[0]);
	c0[0] = 0xfe; TEST(c0[0]);
	c0[0] = 2; TEST(c0[0]);
	c0[0] = 0x55; TEST(c0[0]);

	/* from short */
	s0[0] = -2; TEST(s0[0]);
	s0[0] = 0xfffe; TEST(s0[0]);
	s0[0] = 2; TEST(s0[0]);
	s0[0] = 0x5555; TEST(s0[0]);

	/* from int */
	i0[0] = -2; TEST(i0[0]);
	i0[0] = 0xfffffffe; TEST(i0[0]);
	i0[0] = 2; TEST(i0[0]);
	i0[0] = 0x55555555; TEST(i0[0]);

	/* from long long */
	l0[0] = -2; TEST(l0[0]);
	l0[0] = 0xfffffffffffffffe; TEST(l0[0]);
	l0[0] = 2; TEST(l0[0]);
	l0[0] = 0x5555555555555555; TEST(l0[0]);

	/* from unsigned char */
	C0[0] = -2; TEST(C0[0]);
	C0[0] = 0xfe; TEST(C0[0]);
	C0[0] = 2; TEST(C0[0]);
	C0[0] = 0x55; TEST(C0[0]);

	/* from unsigned short */
	S0[0] = -2; TEST(S0[0]);
	S0[0] = 0xfffe; TEST(S0[0]);
	S0[0] = 2; TEST(S0[0]);
	S0[0] = 0x5555; TEST(S0[0]);

	/* from unsigned int */
	I0[0] = -2; TEST(I0[0]);
	I0[0] = 0xfffffffe; TEST(I0[0]);
	I0[0] = 2; TEST(I0[0]);
	I0[0] = 0x55555555; TEST(I0[0]);

	/* from unsigned long long */
	L0[0] = -2; TEST(L0[0]);
	L0[0] = 0xfffffffffffffffe; TEST(L0[0]);
	L0[0] = 2; TEST(L0[0]);
	L0[0] = 0x5555555555555555; TEST(L0[0]);

	/* from other constant expressions */
	TEST((signed char)(-1));
	TEST((short)(-1));
	TEST((int)(-1));
	TEST((long long)(-1));
	TEST((unsigned char)(-1));
	TEST((unsigned short)(-1));
	TEST((unsigned int)(-1));
	TEST((unsigned long long)(-1));

	TEST((long long)(signed char)(-1));
	TEST((long long)(short)(-1));
	TEST((long long)(int)(-1));
	TEST((long long)(long long)(-1));
	TEST((long long)(unsigned char)(-1));
	TEST((long long)(unsigned short)(-1));
	TEST((long long)(unsigned int)(-1));
	TEST((long long)(unsigned long long)(-1));

	/* from other expressions */
	l0[0] = -1;
	TEST((signed char)l0[0]);
	TEST((short)l0[0]);
	TEST((int)l0[0]);
	TEST((long long)l0[0]);
	TEST((unsigned char)l0[0]);
	TEST((unsigned short)l0[0]);
	TEST((unsigned int)l0[0]);
	TEST((unsigned long long)l0[0]);

	TEST((long long)(signed char)l0[0]);
	TEST((long long)(short)l0[0]);
	TEST((long long)(int)l0[0]);
	TEST((long long)(long long)l0[0]);
	TEST((long long)(unsigned char)l0[0]);
	TEST((long long)(unsigned short)l0[0]);
	TEST((long long)(unsigned int)l0[0]);
	TEST((long long)(unsigned long long)l0[0]);

	exit (0);
}
