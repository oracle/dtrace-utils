/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Positive enumeration test
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */

#pragma D option quiet

enum my_enum {
	zero,
	one = 1,
	two,
	three,
	four = 4,
	minimum = -2147483648,
	maximum = 2147483647
};

BEGIN
{
	exit(0);
}
