/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION:
 * The typedef keyword throws an D_SYNTAX error when a bad existing type is
 * used.
 *
 * SECTION: Type and Constant Definitions/Typedef
 *
 */

#pragma D option quiet


typedef pint new_int;

BEGIN
{
	exit(0);
}
