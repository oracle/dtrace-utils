/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints an array correctly.
 *
 * SECTION: Actions and Subroutines/trace()
 */

BEGIN
{
	trace(curthread->comm);
	exit(0);
}
