/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./172provider_ip.d
 *
 *  DESCRIPTION
 *    The ip provider can be used to study IP communications.
 *    Here is an example that tracks sends and receives,
 *    reporting their source and destination addresses for
 *    10 seconds.
 *
 *    Consult the documentation for further information about
 *    ip probe arguments.
 */

ip:::send,
ip:::receive
{
	printf("%s: from %s to %s\n", probename, args[2]->ip_saddr, args[2]->ip_daddr);
}

tick-10s
{
	exit(0);
}
