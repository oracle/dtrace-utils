/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: The 'probemod' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/probemod
 */

#pragma D option quiet

BEGIN {
	trace(probemod);
	exit(probemod != -1 ? 0 : 1);
}
