/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The setopt() action requires a string as first argument.
 *
 * SECTION: Actions and Subroutines/setopt()
 *
 */

BEGIN
{
	setopt(1);
	exit(0);
}
