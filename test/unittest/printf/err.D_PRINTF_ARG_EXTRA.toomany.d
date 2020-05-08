/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test printf() with too many arguments.
 *
 * SECTION: Output Formatting/printf()
 *
 */

BEGIN
{
	printf("x = %d y = %d\n", 123, 456, 789);
}
