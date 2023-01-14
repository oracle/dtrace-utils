/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Ensure tstrings are handled correctly for ternary (?:) expressions.
 */

#pragma D option quiet

BEGIN {
	a = 1;
	b = 2;
	trace(a > b ? strjoin("abc", "def") : strjoin("ABC", "DEF"));
	trace(" ");
	trace(a < b ? strjoin("abc", "def") : strjoin("ABC", "DEF"));
	trace(" ");
	trace(a == b ? strjoin("abc", "def") : strjoin("ABC", "DEF"));

	exit(0);
}

ERROR {
	exit(1);
}
