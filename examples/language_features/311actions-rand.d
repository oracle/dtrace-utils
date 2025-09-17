/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./311actions-rand.d
 *
 *  DESCRIPTION
 *    The rand() function generates a random integer.
 */

BEGIN
{
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);
	printf(" %3d\n", rand() & 0xff);

	exit(0);
}
