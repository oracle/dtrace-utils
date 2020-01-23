/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option strsize=1024
#pragma D option bufsize=512

BEGIN
{
	exit(0);
}

END
{
	trace("Harding");
	trace("Hoover");
	trace("Nixon");
	trace("Bush");
}
