/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet


BEGIN
{
	a = 7;
	b = 13;
	val = (-a * b) + a;
}

tick-1ms
{
	incr = val % b;
	val += a;
}

tick-1ms
/val == 0/
{
	val += a;
}

tick-1ms
/incr != 0/
{
	i++;
	@[i] = quantize(0, incr);
}

tick-1ms
/incr == 0/
{
	printa(@);
	exit(0);
}
