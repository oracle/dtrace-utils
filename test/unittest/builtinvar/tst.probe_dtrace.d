/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Built-in variables listing provider, module, function, and name
 * are correct from dtrace-provider probes.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

/* Check build-in variables from the dtrace-provider probes. */
BEGIN,ERROR,END
{
	printf("%s:%s:%s:%s\n", probeprov, probemod, probefunc, probename);
}

/* Cause the ERROR probe to fire. */
BEGIN
{
	*((int*)0);
}

/* Cause the END probe to fire. */
BEGIN,ERROR
{
	exit(0);
}
