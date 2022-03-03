/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION: mutex_type_adaptive() should handle too few args passed
 *
 * SECTION: Actions and Subroutines/mutex_type_adaptive()
 */

BEGIN
{
	mutex_type_adaptive();
	exit(1);
}
