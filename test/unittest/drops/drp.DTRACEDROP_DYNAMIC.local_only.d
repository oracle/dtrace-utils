/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Ensure that when (at the end of a work loop) a local-only update is done, we
 * do not end up reporting (unsigned long long)-1.
 *
 * If this bug exists, output will look like:
 *	dtrace: [DTRACEDROP_DYNAMIC] 1 dynamic variable drop
 *	dtrace: [DTRACEDROP_DYNAMIC] 18446744073709551615 dynamic variable drops
 */

#pragma D option dynvarsize=15

BEGIN
{
	self->a = 1;
	self->b = 2;
	self->c = 3;
	self->d = 4;
}

tick-1s
{
	exit(0);
}
