/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	@one["foo", 789, "bar", curthread] = sum(123);
	@two["foo", 789, "bar", curthread] = sum(456);
	printa("%10s %10d %10s %@10d %@10d\n", @one, @two);
	exit(0);
}
