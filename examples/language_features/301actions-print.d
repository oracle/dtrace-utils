#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./301actions-print.d
 *
 *  DESCRIPTION
 *    There are multiple ways to examine data values.
 */

BEGIN
{
	/* printf() is a C-like formatted print */
	/* note:  %d can be used with any size of integer */
	printf("%c %d %s\n", 'a', 1234ll, "hello");

	/* trace() uses data-type information to format */
	trace('a');
	trace(1234);
	trace("hello");

	/* tracemem() dumps a specified number of bytes */
	tracemem(`linux_banner, 64);

	/*
	 * print() uses data-type information to print and annotate
	 * contents at a pointer
	 */
	print(curthread);

	exit(0);
}
