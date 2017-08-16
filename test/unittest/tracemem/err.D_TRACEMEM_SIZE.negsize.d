/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test tracemem() with a negative size argument.
 *
 * SECTION: Actions and Subroutines/tracemem()
 */

BEGIN
{
	tracemem(123, -456);
}
