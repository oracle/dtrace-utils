/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The size of the inbuilt variable NULL is the same as that of an
 * integer.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet

BEGIN
{
	printf("sizeof(NULL): %d\n", sizeof(NULL));
	exit(0);
}

END
/4 != sizeof(NULL)/
{
	exit(1);
}
