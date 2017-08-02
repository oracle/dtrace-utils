/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
/curpsinfo->pr_gid == gid/
{
	exit(0);
}

BEGIN
{
	printf("%d != %d\n", curpsinfo->pr_gid, gid);
	exit(1);
}
