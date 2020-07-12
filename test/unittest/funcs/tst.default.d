/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 * 	Verify that empty clauses are OK.
 *
 * SECTION:  Actions and Subroutines/Default Action
 */


#pragma D option quiet

BEGIN
{
	i = 1;

}

syscall:::entry
{


}

tick-1
/i != 0/
{
	exit(0);
}


END
{

}

