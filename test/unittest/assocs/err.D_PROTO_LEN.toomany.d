/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test an associative array reference that is invalid because of too many
 *   arguments -- this should produce a syntax error at compile time.
 *
 * SECTION: Variables/Associative Arrays
 *
 */

BEGIN
{
	x[123, 456] = timestamp;
}

END
{
	x[123, 456, 789] = timestamp;
}
