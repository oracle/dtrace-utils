/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * signed and unsigned may only be used with integer type
 *
 * SECTION: Errtags/D_DECL_SIGNINT
 *
 */

#pragma D option quiet

/*DSTYLED*/
unsigned struct mystruct {
	int i;
};

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
