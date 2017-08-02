/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the printa() format string in its simplest form.
 *
 * SECTION: Output Formatting/printa()
 */

#pragma D option quiet

BEGIN
{
	@a = count();
	printa("%@u", @a);
	exit(0);
}
