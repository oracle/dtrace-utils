/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: testprobe */

/*
 * ASSERTION:
 *
 * Call matching probe clauses with given name using "*"
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */


#pragma D option quiet

int i;
BEGIN
{
	i = 0;
}

syscall::*exit*:entry
{
	trace(i);
	exit(0);
}
