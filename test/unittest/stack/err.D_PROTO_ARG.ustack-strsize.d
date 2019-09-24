/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test ustack() with an invalid strsize argument.
 *
 * SECTION: Output Formatting/printf()
 *
 */

BEGIN
{
	ustack(1, "i'm not an integer constant");
}
