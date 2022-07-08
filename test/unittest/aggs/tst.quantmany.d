/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

int64_t val, shift;

tick-1ms
/val == 0/
{
	val = -(1 << shift);
	shift++;
}

tick-1ms
/shift == 32/
{
	exit(0);
}

tick-1ms
/val == -1/
{
	val = (1 << shift);
}

tick-1ms
{
	@[shift] = quantize(val, val);
	val >>= 1;
}
