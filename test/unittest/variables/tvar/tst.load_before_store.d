/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that a load-before-store pattern for a thread-local variable
 *	      does not cause the script to fail.
 *
 * SECTION: Variables/Thread-Local Variables
 */

BEGIN
{
	++self->x;
	exit(0);
}
