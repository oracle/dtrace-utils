/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test setopt() with no arguments.
 *
 * SECTION: Actions and Subroutines/setopt()
 *
 */

BEGIN
{
	setopt();
	exit(0);
}
