/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: If aggsize is too small to hold a single aggregation, report an
 *	      error.
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option aggsize=8

BEGIN
{
	@ = count();
	exit(0);
}

ERROR
{
	exit(0);
}
