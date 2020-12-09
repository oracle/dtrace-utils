/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive avg() test using negative values
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is a simple verifiable positive test of the avg() function.
 */

#pragma D option quiet

BEGIN
{
	@ = avg(0);
	@ = avg(-900);
	exit(0);
}
