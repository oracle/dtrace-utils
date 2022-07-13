/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The copyinstr() size must not exceed the (adjusted) scratchsize.
 *
 * SECTION: Actions and Subroutines/copyinstr()
 *	    User Process Tracing/copyin() and copyinstr()
 */

#pragma D option scratchsize=64

BEGIN
{
	copyinstr(0, 65);
	exit(0);
}

ERROR
{
	exit(1);
}
