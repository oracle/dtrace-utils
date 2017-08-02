/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@no-xfail */

#pragma D option quiet
#pragma D option strsize=16

BEGIN
{
	this->str = ",,,Carrots,,Barley,Oatmeal,,,,,,,,,,,,,,,,,,Beans,";
}

BEGIN
{
	strtok(this->str, ",");
}

BEGIN
{
	this->str = ",,,,,,,,,,,,,,,,,,,,,,Carrots,";
	strtok(this->str, ",");
}

BEGIN
{
	strtok(this->str, "a");
}

BEGIN
{
	printf("%s\n", substr(this->str, 1, 40));
}

BEGIN
{
	printf("%s\n", strjoin(this->str, this->str));
}

BEGIN
{
	this->str1 = ".........................................";
	printf("%d\n", index(this->str, this->str1));
}

BEGIN
{
	printf("%d\n", rindex(this->str, this->str1));
}

BEGIN
{
	exit(0);
}
