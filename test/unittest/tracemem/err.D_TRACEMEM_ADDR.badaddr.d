/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test tracemem() with an invalid address argument.
 *
 * SECTION: Actions and Subroutines/tracemem()
 */

BEGIN
{
	tracemem(*curthread, 123);
	exit(0);
}

ERROR
{
	exit(1);
}
