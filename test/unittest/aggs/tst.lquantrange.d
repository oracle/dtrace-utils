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
	@["Nixon"] = lquantize(-20, -10, 10, 1, 25);
	@["Hoover"] = lquantize(20, -10, 10, 1, -100);
	@["Harding"] = lquantize(-10, -10, 10, 1, 15);
	@["Bush"] = lquantize(10, -10, 10, 1, 150);

	printa(@);
	exit(0);
}
