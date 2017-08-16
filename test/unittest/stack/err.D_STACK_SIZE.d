/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test stack() with an invalid argument.
 *
 * SECTION: Output Formatting/printf()
 *
 */

BEGIN
{
	stack("i'm not an integer constant");
}
