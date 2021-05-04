/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	this->s = "foo";
	this->a = 9;
	this->b = -2;

	system("echo %s %d %d", this->s, this->a, this->b);
	system("echo %s %d %d", "bar", this->a, this->b);
	system("echo %s %d", probename, ++this->a);
	exit(0);
}
