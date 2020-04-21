/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test a clause that has a body and a predicate where the
 *            predicate must be cooked last because the clause creates
 *            a variable which is referenced in the predicate.
 *
 * SECTION:   Program Structure / Probe Clauses and Declarations
 *
 */

BEGIN
/x == 0/
{
	exit(x++);
}
