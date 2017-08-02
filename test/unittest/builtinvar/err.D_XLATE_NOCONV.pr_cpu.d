/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * print non assigned value and make sure it fails.
 *
 * SECTION: Variables/Built-in Variables
 *
 *
 */

#pragma D option quiet

BEGIN
{
	printf("The cpu-usage setting = %c\n", curlwpsinfo->pr_cpu);
	exit (0);
}
