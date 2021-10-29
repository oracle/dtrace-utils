/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  Test rand()
 *
 * SECTION: Actions and Subroutines/rand()
 */

BEGIN
{
	i = 0;
}

tick-1
/i != 10/
{
	i++;
	trace(rand());
}

tick-1
/i == 10/
{
	exit(0);
}

