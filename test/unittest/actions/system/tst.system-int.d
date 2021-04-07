/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	this->a = 9;
	this->b = -2;

	system("echo %d %d", this->a, this->b);
	system("echo %d", ++this->a);
	system("echo %d", ++this->a);
	system("echo %d", ++this->a);
	exit(0);
}
