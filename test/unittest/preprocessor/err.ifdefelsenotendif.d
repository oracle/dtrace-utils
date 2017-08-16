/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Call ifdef statement without endif statement.
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#if !defined (FLAG)
#define RETURN 5
#else
#define RETURN 10


#pragma D option quiet

tick-10ms
{
	printf("The value is %d\n", RETURN);
	exit(0);
}
