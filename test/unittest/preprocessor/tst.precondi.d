/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Simple #define test.
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#define value 5


#pragma D option quiet

tick-10ms
/value == 5/
{
	printf("The value is %d\n", value);
	exit(0);
}
