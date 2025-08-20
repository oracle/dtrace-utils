#!/usr/sbin/dtrace -Cqs

/*
 *  SYNOPSIS
 *    sudo ./011intro-preprocessor.d
 *
 *  DESCRIPTION
 *    D scripts can also use the C preprocessor.  This requires using
 *    the "dtrace -C" option, or by adding 'C' to the first, shebang
 *    line, as illustrated above.
 */

#define KEYS "abcde", 12
#define DEBUG

dtrace:::BEGIN
{
#ifdef DEBUG
	printf("\nabcde, 12 should get a count of 1\n");
#endif

	@[KEYS] = count();

	exit(0);
}
