/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option strsize=1024
#pragma D option bufsize=512

BEGIN
{
	trace("Harding");
	trace("Hoover");
	trace("Nixon");
	trace("Bush");
}

BEGIN
{
	exit(0);
}
