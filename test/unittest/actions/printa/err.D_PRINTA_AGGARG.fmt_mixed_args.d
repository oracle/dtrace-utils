/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: printa() accepts only aggregation arguments after the format
 *	string
 *
 * SECTION: Output Formatting/printa()
 */

BEGIN
{
	@a = count();
	@b = max(1);
	@c = min(1);
	printa("%@d", @a, @b, @c, 1);
	exit(0);
}
