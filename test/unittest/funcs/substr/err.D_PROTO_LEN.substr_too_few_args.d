/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine requires at least 2 arguments.
 *
 * SECTION: Actions and Subroutines/substr()
 */

BEGIN
{
	trace(substr(probename));
	exit(0);
}
