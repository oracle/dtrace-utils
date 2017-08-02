/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Print non assigned variables and make sure it fails.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
	printf("The low value is high priority field = %d\n",
		curlwpsinfo->pr_oldpri);
	exit (0);
}
