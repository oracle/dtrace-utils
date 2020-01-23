/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option destructive

BEGIN
{
	self->spec = speculation();
	speculate(self->spec);
}

BEGIN
{
	this->one = 1;
	this->two = 2;
	chill(1);
	speculate(self->spec);
}

BEGIN
{
	speculate(self->spec);
}

BEGIN
{
	exit(0);
}
