#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./006intro-associative.d
 *
 *  DESCRIPTION
 *    The D language also supports associative arrays.
 */

/* associative arrays can be declared explicitly by key (if any) and value types */
int exp[string, int];

BEGIN
{
	/* associative arrays can also be declared implicitly */
	imp["hello", 5] = 1234;

	/* values not set are assumed to be 0 */
	printf("set value is %d; unset value is %d\n",
	    imp["hello", 5],
	    exp["hello", 5]);

	/* when you are done, set a value back to 0 to free the memory */
	imp["hello", 5] = 0;

	/*
	 * By the way, you cannot promote a char associative array
	 * to a string -- whether with stringof or (string) -- since
	 * it is not a consecutive block of memory holding chars in sequence.
	 */

	exit(0);
}
