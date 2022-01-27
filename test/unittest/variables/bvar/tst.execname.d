/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'execname' variable value can be retrieved.
 *
 * SECTION: Variables/Built-in Variables/execname
 */

#pragma D option quiet

BEGIN {
	trace(execname);
	exit(0);
}

ERROR {
	exit(1);
}
