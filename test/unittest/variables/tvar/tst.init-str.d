/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Thread-local string variables are initialized to 0.  Trying to
 *	      trace() it will result in a 'invalid address 0x0' fault.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option quiet

self string x;

BEGIN
{
	trace(self->x);
	exit(1);
}

ERROR
{
	exit(arg4 != 1 || arg5 != 0);
}
