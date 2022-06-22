/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The copyinto() size must not exceed the allocated space.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

BEGIN
{
	ptr = (char *)alloca(64);
	copyinto(curthread->mm->env_start, 65, ptr);
	exit(0);
}

ERROR
{
	exit(1);
}
