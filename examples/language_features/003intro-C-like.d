#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./003intro-C-like.d
 *
 *  DESCRIPTION
 *    Much of D syntax is very C-like.  Examples are shown here.
 *    Check the documentation for more complete information.
 *
 *    Differences include:
 *    - D variables can be typed implicitly
 *    - D has no "if" "switch" "case" "for"
 *    - D has no function definitions
 */

/*
 * As you have already seen, one can use multi-line comments in D
 * similarly to C.
 */

/*
 * Variables can be declared explicitly.  Data types like short, int, long,
 * long long, char, struct, and union are familiar from C.
 */
int x, y, arr[5];

dtrace:::BEGIN
{
	/* Assignments are similar to C. */
	x = 1;
	y = 2;
	x += 3;

	/* There are familiar operators.  E.g., arithmetic operators. */
	x = x + y;
	y = 3 * x - 2 * y;

	/* Relational operators. */
	x = 2 > 3;
	y = 2 >= 3;

	/* Logical operators. */
	x = (2 > 3) && (2 >= 3);

	/* Bitwise operators. */
	x = x & y;

	/* Various unary operators. */
	x = x++ + --y;

	/* Types and casting. */
	x = (long long) (sizeof(int) + ((int) 0xfedcba9876543210ull));

	/*
	 * In contrast with C, D has a printf() whose %d, %x, and %i format
	 * conversions use type information about the corresponding argument
	 * to interpret that argument with its actual word size.
	 */
	printf("%x\n", x);

	/* Arrays can also be associative.  More on that later. */
	arr[0] = 10000;
	arr[1] = 11111;
	arr[2] = 12222;
	arr[3] = 13333;
	arr[4] = 14444;

	/* One can get addresses and dereference addresses. */
	/* (In this case, `nr_cpu_ids is a kernel variable.) */
	ptr = &`nr_cpu_ids;
	printf("nr_cpu_ids:  %d should equal %d\n", *ptr, `nr_cpu_ids);

	exit(0);
}
