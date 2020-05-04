/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a unsigned 32-bit value correctly in
 *            non-quiet mode.
 *
 * SECTION: Actions and Subroutines/trace()
 */

BEGIN
{
	trace((uint32_t)-1);
	exit(0);
}
