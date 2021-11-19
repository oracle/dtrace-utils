/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Thread-local variables are initialized to zero.  This test will
 *	      report failure when 'x' is not zero.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option quiet

self int x;

BEGIN
{
	exit(self->x);
}
