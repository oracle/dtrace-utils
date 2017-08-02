/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2012, Oracle and/or its affiliates. All rights reserved.
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
	self->x[123] = *`cad_pid;
	self->x[456] = `max_pfn;
}
