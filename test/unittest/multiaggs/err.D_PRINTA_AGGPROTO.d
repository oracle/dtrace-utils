/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

BEGIN
{
	@foo[123] = sum(123);
	@bar["fooey"] = sum(456);

	printa("%10d %@10d %@10d\n", @foo, @bar);
	exit(1);
}
