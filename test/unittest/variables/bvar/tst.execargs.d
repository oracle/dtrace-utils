/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'execargs' variable value can be retrieved.
 *
 * SECTION: Variables/Built-in Variables/execargs
 */

#pragma D option quiet

BEGIN {
	trace(execargs);
	exit(0);
}

ERROR {
	exit(1);
}
