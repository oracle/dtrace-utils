/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the kinds of probe definition clause grammar rules.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations;
 * 	Program Structure/Actions
 */


BEGIN
{}

BEGIN
/1/
{}

tick-1
{
	exit(0);
}

