/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: reg spill-fill problems */

#pragma D option quiet

BEGIN {
	A = "000";
	B = "111";
	C = "222";
	D = "333";
	E = "444";
	F = "555";
	trace(strjoin(A, strjoin(B, strjoin(C, strjoin(D, strtok(E, F))))));
	exit(0);
}
