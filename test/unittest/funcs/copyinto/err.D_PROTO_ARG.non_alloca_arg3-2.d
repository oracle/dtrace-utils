/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Argument 3 for copyinto() must be an alloca() pointer.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

BEGIN
{
	copyinto(1, 2, &`max_pfn);
	exit(1);
}

ERROR
{
	exit(0);
}
