/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

struct foo {
	int x;
	char y;
	short z;
};

this struct foo bar;
long bignum;

BEGIN
{
	this->bar.x = 1;
	this->bar.y = ',';
	this->bar.z = 1234;
}

BEGIN
{
	printf("Die %s%c %s.\n", this->bar.x == 1 ? "SystemTap" : "DTrace",
	    this->bar.y, this->bar.z == 1234 ? "Die" : "The");
	exit(0);
}
