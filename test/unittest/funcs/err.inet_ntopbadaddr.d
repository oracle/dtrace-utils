/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

ipaddr_t *ip4a;

BEGIN
{
	ip4a = 0;

	printf("%s\n", inet_ntop(AF_INET, ip4a));

	exit(0);
}

ERROR
{
	exit(1);
}
