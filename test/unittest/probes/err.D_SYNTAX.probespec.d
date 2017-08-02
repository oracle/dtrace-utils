/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Try Declaring Datatypes and variables next to profile declaration and
 * Make sure it results in compilation error.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */


#pragma D option quiet

profile-1:int i
{
	exit(0);
}
