/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

tick-10ms
/i++ < 10/
{
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
}

tick-10ms
/i == 10/
{
	exit(0);
}

END
{
	normalize(@, 5);
	printa(@);
}
