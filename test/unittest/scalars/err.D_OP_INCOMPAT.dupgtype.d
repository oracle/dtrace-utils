/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test assigning a variable two different incompatible types.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */

BEGIN
{
	x = curthread;
	x = pid;
}
