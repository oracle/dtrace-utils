/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  Test functions declarations with varargs
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 */

#pragma D option quiet

extern int varargs1(...);
extern int varargs2(int, ...);

BEGIN
{
	exit(0);
}
