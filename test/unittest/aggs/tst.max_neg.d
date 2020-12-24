/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive max() test using negative values
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is verifiable simple positive test of the max() function.
 */

#pragma D option quiet

BEGIN
{
	@ = max(-100);
	@ = max(-900);
	exit(0);
}
