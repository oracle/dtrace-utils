/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Clause local variables are initialized to zero.  This test will
 *	      report failure when 'x' is not zero.
 *
 * SECTION: Variables/Clause-Local Variables
 */

#pragma D option quiet

this int x;

BEGIN
{
	exit(this->x);
}
