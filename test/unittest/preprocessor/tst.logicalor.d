/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * if !defined (logical-or) usage.
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#if !defined (FLAG) || !defined(STATUS)
#define VALUE 0
#else
#define VALUE 1
#endif

#pragma D option quiet

tick-10ms
{
	printf("The value is %d\n", VALUE);
	exit(0);
}
