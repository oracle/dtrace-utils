/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Calculate timestamp between BEGIN and END.
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

END
{
	total = timestamp - start;
}

BEGIN
{
	start = timestamp;
	exit(0);
}
