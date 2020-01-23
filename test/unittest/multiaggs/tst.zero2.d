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
	@bop[345] = quantize(0, 0);
	@baz[345] = lquantize(0, -10, 10, 1, 0);
}

BEGIN
{
	@foo[123] = sum(123);
	@bar[456] = sum(456);

	@foo[789] = sum(789);
	@bar[789] = sum(789);

	printa("%10d %@10d %@10d\n", @foo, @bar);
	printa("%10d %@10d %@10d %@10d\n", @foo, @bar, @bop);
	printa("%10d %@10d %@10d %@10d %@10d\n", @foo, @bar, @bop, @baz);

	exit(0);
}
