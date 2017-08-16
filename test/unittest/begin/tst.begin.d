/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Order of provider flow, Begin, tick profile, and END.
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

END
{
	printf("End fired after exit\n");
}

BEGIN
{
	printf("Begin fired first\n");
}

tick-1ms
{
	printf("tick fired second\n");
	printf("Call exit\n");
	exit(0);
}
