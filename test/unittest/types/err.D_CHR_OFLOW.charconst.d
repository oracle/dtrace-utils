/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	verify the use of multi-char constants in single quotes
 *
 * SECTION: Types, Operators, and Expressions/Constants
 */

#pragma D option quiet


BEGIN
{
	char_bad = 'abc\fefghi';

	printf("decimal value = %d", char_bad);

	exit(0);
}
