/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Aggregations are not data-recording.
 *
 * SECTION: Aggregations/Aggregations
 */

tick-100ms
{
	@a = count();
}
tick-1005ms
{
	exit(0);
}
