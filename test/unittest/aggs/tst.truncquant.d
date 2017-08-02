/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

int i;

tick-10ms
/i < 100/
{
	@[i] = lquantize(i, 0, 150);
	@[i] = lquantize(i + 1, 0, 150);
	@[i] = lquantize(i + 2, 0, 150);
	@[i] = lquantize(i + 3, 0, 150);
	i++;
}

tick-10ms
/i == 100/
{
	exit(0);
}

END
{
	trunc(@, 5);
}
