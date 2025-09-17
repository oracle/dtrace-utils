/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./009intro-pragmas.d
 *
 *  DESCRIPTION
 *    D options can be set on the command line.  E.g.,
 *
 *        sudo /usr/sbin/dtrace -xq ...
 *        sudo /usr/sbin/dtrace -xw ...
 *        sudo /usr/sbin/dtrace -xstrsize=12 ...
 *
 *    They can also be set via pragmas inside a script.
 *    Respectively,
 */

/* -q */
#pragma D option quiet

/* -w, e.g., for the system() call */
#pragma D option destructive

/* -xstrsize=12 */
#pragma D option strsize=12

/*
 * Simple options could alternatively be set on the first
 * "shebang" line.  E.g., "#!/usr/sbin/dtrace -qws".
 */

dtrace:::BEGIN
{
	/* string truncated to strsize */
	system("echo abcdefghijklmnopqrstuvwxyz");

	exit(0);
}
