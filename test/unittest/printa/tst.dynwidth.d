/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	/*
	 * Yes, this works.  But is it useful?  And is it useful to someone
	 * other than jwadams and/or dep?
	 */
	@[10, 100] = sum(1);
	@[-10, 200] = sum(2);

	printa("-->%-*d<--\n", @);
	printa("-->%*d<--\n", @);
	exit(0);
}
