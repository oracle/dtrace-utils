/*
 * Oracle Linux DTrace.
 * Copyright © 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

uint8_t *hwaddr;

BEGIN
{
	hwaddr = 0;

	printf("%s\n", link_ntop(ARPHRD_ETHER, hwaddr));

	exit(0);
}

ERROR
{
	exit(1);
}
