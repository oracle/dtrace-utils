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
	trace("Harding");
	trace("Hoover");
}

BEGIN
{
	trace("Nixon");
	trace("Bush");
}

BEGIN
{
	exit(0);
}
