/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that post-increment on an local variable evaluates to the
 *            old value of the local variable.
 *
 * SECTION: Variables/Clause-Local Variables
 */

BEGIN
{
	trace(this->x++);
	exit(0);
}
