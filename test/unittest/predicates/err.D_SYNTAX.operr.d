/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple if,then operations test.
 *	Call impossible conditions and make sure compilation fails.
 *
 * SECTION: Program Structure/Predicates
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

profile-1
/i != 0 && >= 5 || <= 10/
{
	i++;
	trace(i);
}
