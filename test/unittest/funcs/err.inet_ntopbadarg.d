/*
 * Oracle Linux DTrace.
 * Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

ipaddr_t *ip4a;

BEGIN
{
	this->buf4a = alloca(sizeof (ipaddr_t));
	ip4a = this->buf4a;
	*ip4a = htonl(0xc0a80117);

	printf("%s\n", inet_ntop(-1, ip4a));

	exit(0);
}

ERROR
{
	exit(1);
}
