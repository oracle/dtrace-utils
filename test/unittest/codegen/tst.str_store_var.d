/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option rawbytes
#pragma D option strsize=6
#pragma D option quiet

string	x;

BEGIN
{
	x = "abcdefgh";
	trace(x);
	x = "12";
	trace(x);
	exit(0);
}

ERROR
{
	exit(1);
}
