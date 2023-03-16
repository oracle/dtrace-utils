/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option strsize=1k
#pragma D option bufsize=3k

BEGIN
{
	exit(0);
}

END
{
	trace("Harding");
	trace("Hoover");
}

END
{
	trace("Nixon");
	trace("Bush");
}
