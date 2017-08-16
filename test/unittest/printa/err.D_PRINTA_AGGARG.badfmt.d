/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Invokes printa() with a bad format string.
 *
 * SECTION: Output Formatting/printa()
 *
 */

BEGIN
{
	printa(123);
}
