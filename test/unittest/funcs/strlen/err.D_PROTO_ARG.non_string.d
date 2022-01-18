/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The argument to strlen() should be a string.
 *
 * SECTION: Actions and Subroutines/strlen()
 */

BEGIN
{
	strlen(12);
	exit(0);
}
