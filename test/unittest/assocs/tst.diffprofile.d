/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * To test Clause Local Variables ' this' across different profiles.
 *
 * SECTION: Variables/Associative Arrays
 *
 *
 */

#pragma D option quiet

this int x;
this char c;

tick-10ms
{
	this->x = 123;
	this->c = 'D';
	printf("The value of x is %d\n", this->x);
}

tick-10ms
{
	this->x = 235;
	printf("The value of x is %d\n", this->x);
}

tick-10ms
{
	this->x = 456;
	printf("The value of x is %d\n", this->x);
	exit(0);
}
