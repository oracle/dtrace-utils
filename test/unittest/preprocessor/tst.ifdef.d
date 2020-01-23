/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *
 * Simple test to test 'if defined' 'defined' 'else' 'endif'
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#if defined (FLAG)
#define VALUE 5
#else
#define VALUE 10
#endif


#pragma D option quiet

tick-10ms
{
	printf("The value is %d\n", VALUE);
	exit(0);
}
