/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	/* 256-char string ending in "XYZ" */
	x = "_____________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________XYZ";

	/* check whether the last char of a long string is seen */
	y = "a";
	printf("%s\n", strtok(x, y));

	/* check whether the last char of a long delimiter string is seen */
	z = "zyxwvutsrqponmlkjihgfedcbaZYXWVUTSRQPONMLKJIHGFEDCBA";
	printf("%s\n", strtok(z, x));

	/* check that strtok internal state can handle a large offset */
	y = "Z";
	printf("%s\n", strtok(x, y));
	y = "a";
	printf("%s\n", strtok(NULL, y));

	exit(0);
}

ERROR
{
	exit(1);
}
