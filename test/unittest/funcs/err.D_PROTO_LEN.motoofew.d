/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	mutex_owned() should handle too few args passed
 *
 * SECTION: Actions and Subroutines/mutex_owned()
 *
 */

lockstat:genunix:mutex_enter:adaptive-acquire
{
	mutex_owned();
	exit(1);
}
