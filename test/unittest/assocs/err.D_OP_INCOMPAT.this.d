/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare 'this int' variable and assign inappropriate data type.
 *
 * SECTION: Variables/Associative Arrays
 *
 *
 */

#pragma D option quiet

this int x;

BEGIN
{
	this->x = "dummy";
	exit(0);
}
