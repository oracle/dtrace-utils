/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	mutex_type_adaptive() should handle too many args passed
 *
 * SECTION: Actions and Subroutines/mutex_type_adaptive()
 *
 */


lockstat:genunix:mutex_enter:adaptive-acquire
{
	mutex_type_adaptive((kmutex_t *)arg0, 99);
	exit(1);
}
