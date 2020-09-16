/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/*
 * ASSERTION:
 *	progenyof() should accept one argument - a pid
 *
 * SECTION: Actions and Subroutines/progenyof()
 *
 */


BEGIN
{
	progenyof(1, 2);
	exit(0);
}

