/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	Using the scope operator in a declaration will result in a
 *	D_DECL_SCOPE error.
 *
 * SECTION: Variables/External Variables
 *
 */

#pragma D option quiet

int `xyz;

BEGIN
{
	exit(1);
}
