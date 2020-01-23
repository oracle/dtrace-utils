/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the various kinds of D probe description statement
 *   rules in the grammar.
 *
 * SECTION: Program Structure/Probe Descriptions
 *
 */

/* @@xfail: dtv2 */
/* @@timeout: 25 */

BEGIN
{
	i = 0;
}

tick-1
/i != 20/
{
	i++;
	x = 0;
	stack(10);
	@a = count();
	@b = max(x);
	@c[x] = count();
	@d[x] = max(x);
}

tick-1
/i = 20/
{
	exit(0);
}
