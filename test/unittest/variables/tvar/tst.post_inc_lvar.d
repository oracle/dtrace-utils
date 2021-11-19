/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that post-increment on thread-local variable evaluates to
 *            the old value of the variable.
 *
 * SECTION: Variables/Thread-Local Variables
 */

BEGIN
{
	trace(self->x++);
	exit(0);
}
