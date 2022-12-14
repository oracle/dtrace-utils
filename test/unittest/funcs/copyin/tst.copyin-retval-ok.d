/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Ensure the alloca()'d pointer return value of copyin() is valid.
 *
 * SECTION: Actions and Subroutines/copyin()
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("echo dtrace-copyin-test");
}

syscall::write:entry
{
	((uint8_t *)copyin(arg1, 32))[0];
	exit(0);
}

ERROR
{
	exit(1);
}
