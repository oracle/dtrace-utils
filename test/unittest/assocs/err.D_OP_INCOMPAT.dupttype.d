/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test assigning a variable two different incompatible types.  This should
 *   result in a compile-time error.
 *
 * SECTION: Variables/Associative Arrays
 *
 */

BEGIN
{
	self->x[123] = curthread;
	self->x[456] = pid;
}
