/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option rawbytes
#pragma D option strsize=5
#pragma D option quiet

BEGIN
{
	trace("abcdefgh");
	exit(0);
}

ERROR
{
	exit(1);
}
