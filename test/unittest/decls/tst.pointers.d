/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test multiple extern declarations in a single list which build
 *  up a chain of pointers to pointers to pointers ...
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 */

extern int e0, *e1, **e2, ***e3, ****e4, *****e5;

BEGIN
{
	exit(0);
}
