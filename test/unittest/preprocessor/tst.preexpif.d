/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Simple define, if defined, define and endif tests.
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#define FLAG 1
#if defined (FLAG)
#define VALUE 5
#endif


#pragma D option quiet

tick-10ms
{
	printf("The value is %d\n", VALUE);
	exit(0);
}
