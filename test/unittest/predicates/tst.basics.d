/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple if,then operations test.
 *	Call simple expressions and make sure test satisfy conditions.
 *	Match expected output in tst.basics.d.out
 *
 * SECTION: Program Structure/Predicates
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i < 10/
{
	i++;
	trace(i);
}

tick-10ms
/i == 10/
{
	exit(0);
}
