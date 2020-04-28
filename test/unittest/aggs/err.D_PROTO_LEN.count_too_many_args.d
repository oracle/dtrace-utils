/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: *   count() should not accept any arguments
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = count(1);
	exit(0);
}
