/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test the strlen() subroutine.
 *
 * SECTION: Actions and Subroutines/strlen()
 *
 */

BEGIN
/strlen("I like DTrace") == 13/
{
	correct = 1;
}

BEGIN
{
	exit(correct ? 0 : 1);
}
