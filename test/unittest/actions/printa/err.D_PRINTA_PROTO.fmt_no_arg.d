/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: printa() requires an aggregation argument after the format string
 *
 * SECTION: Output Formatting/printa()
 */

BEGIN
{
	printa("%d");
	exit(0);
}
