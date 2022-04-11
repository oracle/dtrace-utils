/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: ternary conditionals with alloca and non-alloca branches
 *            can be used for things other than variable assignment.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	a = (char *) alloca(1);
        trace((a == NULL) ? (char *) 50 : a);
        exit(0);
}

ERROR
{
	exit(1);
}
