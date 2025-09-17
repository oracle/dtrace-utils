/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./000intro-hello-world.d
 *
 *        or
 *
 *    sudo /usr/sbin/dtrace -s 000intro-hello-world.d
 *
 *        or
 *
 *    sudo /usr/sbin/dtrace -n 'dtrace:::BEGIN
 *                              {
 *                                  printf("hello world\n");
 *                                  exit(0);
 *                              }
 *                              dtrace:::END
 *                              {
 *                                  trace("goodbye world");
 *                              }'
 *
 *  DESCRIPTION
 *    This is a simple "hello world" program in D.  It
 *    illustrates a variety of very simple features of D.
 */

/*
 * The BEGIN probe fires when the script is launched.  It can
 * be called by simply asking for BEGIN.  Or, its provider can
 * also be named:  dtrace:::BEGIN.  The other probe description
 * fields, the module and function, are blank for this probe.
 */
dtrace:::BEGIN
{
	/*
	 * A C-like printf() function can be used to print
	 * data.  Alternatively, the trace() function can be
	 * used, and it will discern the appropriate data type.
	 */
	printf("hello world\n");

	/*
	 * Variables can be assigned.  They maybe typed implicitly.
	 */
	x = 123;

	exit(0);
}

dtrace:::BEGIN
/x == 456/    /* A predicate can indicate whether to execute a clause. */
{
	trace("YOU SHOULD NOT SEE THIS MESSAGE!!!\n");
}

/*
 * We can also gives actions to list at the end.
 */
dtrace:::END
{
	trace("goodbye world");
}
