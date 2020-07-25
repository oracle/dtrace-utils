/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

uint64_t now;

BEGIN
{
	now = 18252813184; /* Jan 1, 2004 00:00:00 */
}

BEGIN
/walltimestamp < timestamp/
{
	printf("%d < %d", walltimestamp, timestamp);
	exit(1);
}

BEGIN
/walltimestamp < now/
{
	printf("%d (%Y) is before %Y", walltimestamp, walltimestamp, now);
	exit(2);
}

BEGIN
{
	exit(0);
}
