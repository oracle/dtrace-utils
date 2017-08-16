/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Test multiple count() calls.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i != 10/
{
	i++;
	@counts1[execname] = count();
	@counts2[execname, arg0] = count();
}

tick-10ms
/i == 10/
{
	exit(0);
}
