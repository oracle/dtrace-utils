/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Test the D enumerations with and without initilialization of the identifiers.
 * Also test for values with negative integer assignments, expressions and
 * fractions.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 */

#pragma D option quiet

enum colors {
	RED,
	ORANGE = 5 + 5,
	YELLOW = 2,
	GREEN,
	BLUE = GREEN + ORANGE,
 	PINK = 5/4,
	INDIGO = -2,
	VIOLET
};

profile:::tick-1sec
/(0 == RED) && (10 == ORANGE) && (2 == YELLOW) && (3 == GREEN) &&
    (13 == BLUE) && (1 == PINK) && (-2 == INDIGO) && (-1 == VIOLET)/
{
	exit(0);
}
