/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option strsize=14

BEGIN
{
	/* "Linu" will be 76.105.110.117 */
	printf("%s\n", inet_ntoa((ipaddr_t *)&`linux_banner));
	exit(0);
}

ERROR
{
	exit(1);
}
