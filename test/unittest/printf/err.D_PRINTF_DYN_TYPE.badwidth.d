/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Tet dynamic width argument of non-integer type.
 *
 * SECTION: Output Formatting/printf()
 *
 */

BEGIN
{
	printf("%*d\n", "foo", 1);
}
