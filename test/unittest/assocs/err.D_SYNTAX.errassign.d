/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 *   Assign one to 10 elements; make sure fails to compile.
 *
 * SECTION: Variables/Associative Arrays
 *
 */

BEGIN
{
	x[10]=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
}

END
{
}
