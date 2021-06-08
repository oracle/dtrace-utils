/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet
#pragma D option aggsortkey

/*
 * This is to check that we're correctly null-terminating the result of the
 * substr() subroutine.
 */

tick-1ms
/i++ > 1000/
{
	exit(0);
}

tick-1ms
{
	@[substr((i & 1) ? "Bryan is smart" : "he's not a dummy", 0,
	    (i & 1) ? 8 : 18)] = count();
}

END
{
	printa("%s\n", @);
}
