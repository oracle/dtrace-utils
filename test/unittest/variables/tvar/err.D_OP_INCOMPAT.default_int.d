/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A thread-local variable declared without a type is implicitly
 *	      declared as an int.  Verify that assignment of a string value
 *	      fails.
 *
 * SECTION: Variables/Thread-Local Variables
 */

self x;

BEGIN
{
	self->x = execname;
	exit(0);
}
