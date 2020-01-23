/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

int i;

tick-10ms
{
	@ = quantize(1LL << i);
	@ = quantize((1LL << i) + 1);
	@ = quantize(-(1LL << i));
	@ = quantize(-(1LL << i) - 1);
	i++;
}

tick-10ms
/i == 64/
{
	exit(0);
}
