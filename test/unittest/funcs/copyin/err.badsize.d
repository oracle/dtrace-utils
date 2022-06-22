/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The copyin() size must not exceed the (adjusted) scratchsize.
 *
 * SECTION: Actions and Subroutines/copyin()
 *	    User Process Tracing/copyin() and copyinstr()
 */

#pragma D option scratchsize=64

BEGIN
{
	sz = 65;
	copyin(0, sz);
	exit(0);
}

ERROR
{
	exit(1);
}
