/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf action supports '%g' for double floating point values.
 *
 * SECTION: Actions/printf()
 */

#pragma D option quiet

double d;

BEGIN
{
	printf("%g", d);
	exit(0);
}
