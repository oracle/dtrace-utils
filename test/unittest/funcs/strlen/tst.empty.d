/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: strlen("") is 0
 *
 * SECTION: Actions and Subroutines/strlen()
 */

#pragma D option quiet

BEGIN
{
	exit(strlen(""));
}

ERROR
{
	exit(1);
}
