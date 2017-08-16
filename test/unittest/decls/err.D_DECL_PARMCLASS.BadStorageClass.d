/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * inappropriate storage class for function or associative array parameter
 * throws a D_DECL_PARMCLASS
 *
 * SECTION: Errtags/D_DECL_PARMCLASS
 *
 */

#pragma D option quiet


int foo(static int);

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
