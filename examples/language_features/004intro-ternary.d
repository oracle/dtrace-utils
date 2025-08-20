#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./004intro-ternary.d
 *
 *  DESCRIPTION
 *    Much of D syntax is very C-like.  Another example is
 *    ternary operations:  ($condition ? $if_yes : $if_no).
 *    In D, however, ternary operations are argubly more
 *    useful than they are in C.  The D language does not
 *    have conditional constructs like "if" statements.
 *    Therefore, other constructs, like predicates and
 *    speculations, must be used.  Or, ternary operations.
 */

dtrace:::BEGIN
{
	x = 123;
	y = 456;

	/* basic ternary operation to implement z = MIN(x, y) */
	z = (x < y) ? x : y;

	/* a more novel use to conditionalize an assignment */
	(x < y) ? (y = x) : 1;

	/* all three variables should have the same value */
	printf("x = %d; y = %d; z = %d\n", x, y, z);

	exit(0);
}
