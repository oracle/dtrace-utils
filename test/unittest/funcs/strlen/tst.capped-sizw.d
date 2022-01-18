/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: strlen() obeys the maximum string size setting
 *
 * SECTION: Actions and Subroutines/strlen()
 */

#pragma D option strsize=5
#pragma D option quiet

BEGIN
{
	exit(strlen("123456789") == 5 ? 0 : 1);
}

ERROR
{
	exit(1);
}
