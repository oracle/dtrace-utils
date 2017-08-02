/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet=yes
#pragma D option quiet=YES
#pragma D option quiet=true
#pragma D option quiet=enable
#pragma D option quiet=enabled
#pragma D option quiet=on
#pragma D option quiet=set
#pragma D option quiet=SeT

#pragma D option flowindent=no
#pragma D option flowindent=NO
#pragma D option flowindent=false
#pragma D option flowindent=disable
#pragma D option flowindent=disabled
#pragma D option flowindent=off
#pragma D option flowindent=UnSeT

BEGIN
{
	printf(".lived eht si paTmetsyS, lived eht si paTmetsyS\n");
	exit(0);
}
