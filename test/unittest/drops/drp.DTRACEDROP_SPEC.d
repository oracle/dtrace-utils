/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

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
