/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A thread-local variable declared without a type is implicitly
 *	      declared as an int.
 *
 * SECTION: Variables/Thread-Local Variables
 */

self x;

BEGIN
{
	x = 1;
	exit(0);
}
