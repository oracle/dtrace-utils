/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine accepts no more than three arguments.
 *
 * SECTION: Actions and Subroutines/substr()
 */

BEGIN
{
	trace(substr(probename, 1, 2, 3));
	exit(0);
}
