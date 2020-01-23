/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	ustack() first argument must be a non-zero positive integer constant
 *
 * SECTION: User Process Tracing/ustack();
 *	Actions and Subroutines/ustack()
 *
 */


BEGIN
{
	ustack(0, 200);
	exit(0);
}

