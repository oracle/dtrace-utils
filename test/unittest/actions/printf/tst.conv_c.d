/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf action supports '%c' for 8-bit character values.
 *
 * SECTION: Actions/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("%c", 0x41);
	exit(0);
}
