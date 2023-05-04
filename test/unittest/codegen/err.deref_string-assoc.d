/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	s[1234] = "ABCDEFG";
	trace(s[1234][1]);
}

BEGIN
{
	s[1234] = NULL;
	trace(s[1234][1]);
}

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
