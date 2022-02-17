/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Tuple assembly does not contain garbage from previous uses
 *
 * SECTION: Variables/Associative Arrays
 */

#pragma D option quiet

BEGIN {
	x["foo"] = 1234;
	x["long key her"] = 56789;

	trace(x["foo"]);
	trace(x["long key her"]);

	exit(x["foo"] != 1234 || x["long key her"] != 56789);
}

ERROR
{
	exit(1);
}
