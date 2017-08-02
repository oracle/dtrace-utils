/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Attempt to declare 'ifdef' statement inbetween probe declaration
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */


#pragma D option quiet

tick-10ms
#if !defined (FLAG)
#define VALUE 5
#endif
{

	printf("The value is %d\n", VALUE);
	exit(0);
}
