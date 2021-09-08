/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Enough space is allocated for each string variable.
 *
 * SECTION: Variables/Scalar Variables
 */

#pragma D option quiet
#pragma D option strsize=5

BEGIN
{
	x = "abcd";
	y = "abcd";
	z = "abcd";
	exit(0);
}

END {
	trace(x);
	trace(y);
	trace(z);
}

ERROR
{
	exit(1);
}
