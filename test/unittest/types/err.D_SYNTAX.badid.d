/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Identifiers cannot begin with a digit
 *
 * SECTION: Types, Operators, and Expressions/Identifier Names and Keywords
 */


#pragma D option quiet

int 0abc;

BEGIN
{
	0abc = 5;
	printf("0abc is %d", 0abc);
	exit(0);
}
