/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Aggregations cannot be used in expression context.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@a = count();
	trace(@a + 3);
	exit(0);
}
