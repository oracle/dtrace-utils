/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Exactly three Arguments are required for copyinto().
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

BEGIN
{
	copyin(1, 2, 3, 4);
	exit(1);
}

ERROR
{
	exit(0);
}
