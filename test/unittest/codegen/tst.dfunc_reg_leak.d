/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Ensure that we do not leak registers with void expressions.
 */

#pragma D option quiet

BEGIN {
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	strlen("a");
	exit(0);
}

ERROR {
	exit(1);
}
