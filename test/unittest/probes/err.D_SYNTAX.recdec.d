/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare a probe clause within another probe clause.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */


#pragma D option quiet

profile-1
{
	tick-10ms
{
}
}
