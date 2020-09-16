/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/*
 * ASSERTION:
 *  Invokes printa() with a bad aggregation argument.
 *
 * SECTION: Output Formatting/printa()
 *
 */

BEGIN
{
	printa("%d");
}
