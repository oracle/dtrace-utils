/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: If a program has a set of attributes less than the predefined
 * minimum a D_ATTR_MIN is thrown
 *
 * SECTION: Stability/Stability Enforcement
 * SECTION: Errtags/D_ATTR_MIN
 *
 */

#pragma D option quiet
#pragma D option amin=Evolving/Evolving/Common

BEGIN
{
	trace(curthread->t_procp);
	exit(0);
}

ERROR
{
	exit(0);
}
