/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare pragma inside probe clause.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */



tick-10ms
{
#pragma D option quiet
	exit(0);
}
