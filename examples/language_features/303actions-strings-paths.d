/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

#pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./303actions-strings-paths.d
 *
 *  DESCRIPTION
 *    There are functions to manipulate strings, specifically path names.
 */

BEGIN
{
	/* find the base name of a path */
	printf("base is %s\n", basename("/usr/sbin/dtrace"));

	/* find the directory of a path */
	printf("directory is %s\n", dirname("/usr/sbin/dtrace"));

	/* clean up a path name */
	printf("cleaned up path is %s\n", cleanpath("////usr/bin/../sbin/././//dtrace"));

	exit(0);
}
