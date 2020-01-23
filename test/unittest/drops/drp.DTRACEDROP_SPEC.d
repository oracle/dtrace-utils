/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option nspec=1
#pragma D option specsize=32
#pragma D option strsize=512

BEGIN
{
	spec = speculation();
	speculate(spec);
	trace("The, SystemTap, The.");
}

BEGIN
{
	commit(spec);
}

BEGIN
{
	exit(0);
}
