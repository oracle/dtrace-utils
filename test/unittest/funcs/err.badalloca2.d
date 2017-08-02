/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	alloca() cannot be used to "unconsume" scratch space memory by
 *	accepting a negative offset.
 *
 * SECTION: Actions and Subroutines/alloca()
 *
 */

BEGIN
{
	ptr = alloca(10);
	ptr = alloca(0xffffffffffffffff);
	exit(0);
}

ERROR
{
	exit(1);
}
