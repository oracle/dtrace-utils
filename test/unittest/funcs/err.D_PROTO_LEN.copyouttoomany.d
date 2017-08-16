/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   copyout() should handle too many arguments passed.
 *
 * SECTION: Actions and Subroutines/copyin();
 * 	Actions and Subroutines/copyinstr()
 *
 */


BEGIN
{
	copyout((void *)3, (uintptr_t)1000, (size_t)1000, "toomany");
	exit(0);
}

