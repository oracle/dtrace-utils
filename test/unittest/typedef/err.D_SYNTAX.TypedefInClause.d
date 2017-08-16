/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION:
 * The typedef keyword can be used outside of the probe clauses only.
 *
 * SECTION: Type and Constant Definitions/Typedef
 *
 */

#pragma D option quiet


BEGIN
{
	typedef int new_int;
	exit(0);
}
