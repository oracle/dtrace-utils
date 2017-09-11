/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test setop() with too many arguments.
 *
 * SECTION: Actions and Subroutines/setopt()
 *
 */

BEGIN
{
	setopt("bufsize", "20m", "dummy");
}
