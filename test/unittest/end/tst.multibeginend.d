/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Tests multiple END profile.
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

END
{
	printf("End1 fired after exit\n");
}
END
{
	printf("End2 fired after exit\n");
}
END
{
	printf("End3 fired after exit\n");
}
END
{
	printf("End4 fired after exit\n");
}

BEGIN
{
	printf("Begin fired first\n");
}
BEGIN
{
	printf("Begin fired second\n");
}
BEGIN
{
	printf("Begin fired third\n");
}
BEGIN
{
	printf("Begin fired fourth\n");
}
BEGIN
{
	printf("Begin fired fifth\n");
	exit(0);
}
