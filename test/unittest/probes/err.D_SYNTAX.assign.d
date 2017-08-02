/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare a predicate with no spaces and make sure it results in
 * compilation error.

 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */


#pragma D option quiet

int i = 0;

profile-1
/i == 0/
{
	exit(0);
}
