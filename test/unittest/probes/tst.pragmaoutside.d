/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare pragma outside probe clause.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */



tick-10ms
{
	exit(0);
}
#pragma D option quiet
