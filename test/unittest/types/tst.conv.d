/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	positive type conversion checks
 *
 * SECTION: Types, Operators, and Expressions/Type Conversions
 *
 * NOTES: not all type conversions are checked.  A lot of this section
 * 	is tested within other tests.
 */

#pragma D option quiet

unsigned int i;
char c;
short s;
long l;
long long ll;

BEGIN
{
/* char -> int */
	c = 'A';
	i = c;
	printf("c is %c i is %d\n", c, i);

/* int -> char */

	i = 1601;
	c = i;
	printf("i is %d c is %c\n", i, c);

/* char -> short */
	c = 'A';
	s = c;
	printf("c is %c s is %d\n", c, s);

/* short -> char */

	s = 1601;
	c = s;
	printf("s is %d c is %c\n", s, c);

/* int -> short */

	i = 1601;
	s = i;
	printf("i is %d s is %d\n", i, s);

/* short -> int */

	s = 1601;
	i = s;
	printf("s is %d i is %d\n", s, i);

/* int -> long long */

	i = 4294967295;
	ll = i;
	printf("i is %d ll is %x\n", i, ll);

/* long long -> int */

	ll = 8589934591;
	i = ll;
	printf("ll is %d i is %x\n", ll, i);

/* char -> long long */

	c = 'A';
	ll = c;
	printf("c is %c ll is %x\n", c, ll);

/* long long -> char */

	ll = 8589934401;
	c = ll;
	printf("ll is %x c is %c\n", ll, c);

	exit(0);
}

