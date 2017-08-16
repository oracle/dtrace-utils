/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * long and long long may only be used with integer or floating-point type
 *
 * SECTION: Errtags/D_DECL_LONGINT
 *
 */

#pragma D option quiet

long struct mystruct
{
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
