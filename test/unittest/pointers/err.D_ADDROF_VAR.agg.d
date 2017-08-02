/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test that the & operator fails with D_ADDROF_VAR if its operand
 *  is a dynamic variable such as an aggregation.
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 * SECTION: Pointers and Arrays/Pointers to Dtrace Objects
 */

BEGIN
{
	@a = count();
	trace(&@a);
}
