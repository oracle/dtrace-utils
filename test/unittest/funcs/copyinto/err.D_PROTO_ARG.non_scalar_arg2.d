/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Argument 2 for copyinto() must be a scalar.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

BEGIN
{
	copyinto(1, "2", 3);
	exit(1);
}

ERROR
{
	exit(0);
}
