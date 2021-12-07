/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Stress test some tstring-free cases.
 */

#pragma D option quiet

BEGIN {
	trace(
	    (x = strjoin("abc", "def")) ==
	    (y = strjoin("abc", "def")) ? 0 : 1);
	trace(strjoin(
	    (x = strjoin("abc", "def")),
	    (y = strjoin("ABC", "DEF"))));
	trace(substr(
	    (x = strjoin("123", "456")),
	    2));
	trace(x = strjoin("abc", "def"));
	trace(x =
	    (y = strjoin("ABC", "DEF")));

	exit(0);
}
