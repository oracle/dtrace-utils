/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * void must be sole parameter in a function declaration.
 *
 * SECTION: Errtags/D_DECL_FUNCVOID
 *
 */

#pragma D option quiet

int foo(int, void);

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
