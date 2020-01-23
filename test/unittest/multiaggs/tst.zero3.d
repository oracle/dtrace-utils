/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

BEGIN
/0/
{
	@bop[345] = quantize(0, 0);
	@baz[345] = lquantize(0, -10, 10, 1, 0);
}

BEGIN
{
	printa(@bop);
	printa(@baz);
	printa("%@10d %@10d\n", @bop, @baz);

	exit(0);
}
