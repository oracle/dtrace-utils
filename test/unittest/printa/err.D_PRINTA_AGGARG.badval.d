/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Invokes printa() with an argument that does not match the aggregation
 *  conversion.
 *
 * SECTION: Output Formatting/printa()
 *
 */

BEGIN
{
	printa("%@d\n", 123);
}
