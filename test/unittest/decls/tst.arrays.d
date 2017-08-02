/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test various kinds of array declarations.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 *
 */

extern int a1[];

extern int a2[1];

extern int a3[123][456];

extern int a4[123][456][789];

extern int a5[5], a6[6][6], a7[7][7][7];

BEGIN
{
	exit(0);
}
