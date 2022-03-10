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
 * Copy value from associative arrays to local variables.
 *
 * SECTION: Variables/Associative Arrays
 *
 *
 */

#pragma D option quiet

this int x;

BEGIN
{
	a["abc", 123] = 123;
}

tick-10ms
{
	this->x = a["abc", 123]++;
	printf("The value of x is %d\n", this->x);
}

tick-10ms
{
	this->x = a["abc", 123]++;
	printf("The value of x is %d\n", this->x);
}

tick-10ms
{
	this->x = a["abc", 123]++;
	printf("The value of x is %d\n", this->x);
	exit(0);
}

ERROR
{
	exit(1);
}
