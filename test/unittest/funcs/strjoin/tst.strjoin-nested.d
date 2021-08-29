/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Nested strjoin() works
 *
 * SECTION: Actions and Subroutines/strjoin()
 */

#pragma D option quiet

BEGIN
{
	trace(strjoin(
		strjoin(
			strjoin(probeprov, ":"),
			strjoin(probemod, ":")
		),
		strjoin(
			strjoin(probefunc, ":"),
			probename
		)
	      ));
	exit(0);
}

ERROR
{
	exit(1);
}
