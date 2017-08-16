/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	concat of providers not allowed with BEGIN
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet


BEGIN:tick-1
{
	printf("Begin fired first\n");
	exit(0);
}
