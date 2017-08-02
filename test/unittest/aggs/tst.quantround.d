/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/*
 * This test verifies that the height of the ASCII quantization bars is
 * determined using rounding and not truncated integer arithmetic.
 */
tick-10ms
/i++ >= 27/
{
	exit(0);
}

tick-10ms
/i > 8/
{
	@ = quantize(2);
}

tick-10ms
/i > 2 && i <= 8/
{
	@ = quantize(1);
}

tick-10ms
/i <= 2/
{
	@ = quantize(0);
}
