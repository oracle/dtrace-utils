/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Using an integer constant that cannot be represented in any of the
 * builtin integral types throws a D_INT_OFLOW error.
 *
 * SECTION: Errtags/D_INT_OFLOW
 */

BEGIN
{
	0x12345678123456781;
}
