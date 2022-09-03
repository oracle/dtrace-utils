/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Offset and alignment of an aggregation key element is defined by
 *	      the prototype of the aggregation key and not the passed values.
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

BEGIN
{
	@[(int32_t)1, 2] = count();
	@[(int64_t)1, 2] = count();
	exit(0);
}

ERROR
{
	exit(1);
}
