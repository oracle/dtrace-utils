/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test a clause that has a body and a predicate.
 * SECTION:   Program Structure / Probe Clauses and Declarations
 *
 */

BEGIN
/$pid != 0/
{
	exit(0);
}
