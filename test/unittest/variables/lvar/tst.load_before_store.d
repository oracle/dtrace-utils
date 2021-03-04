/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that a load-before-store pattern for a local variable
 *	      does not cause the script to fail.
 *
 * SECTION: Variables/Clause-Local Variables
 */

BEGIN
{
	++this->x;
	exit(0);
}
