/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Aggregations cannot be called from D expressions.
 *
 * SECTION: Aggregations/Clearing aggregations
 *
 *
 */

BEGIN
{
	avg(1);
	exit(0);
}
