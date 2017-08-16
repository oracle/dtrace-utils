#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv4 UDP to a local address.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. No physical network interface is plumbed and up.
# 3. No other hosts on this subnet are reachable and listening on rpcbind.
# 4. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test sends a UDP message using ping and checks that at least the
# following counts were traced:
#
# 1 x ip:::send (UDP sent to ping's base UDP port)
# 1 x ip:::receive (UDP received)
# 

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=127.0.0.1
$dtrace $dt_cflags -c "$testdir/perlping.pl udp $local" -qs /dev/stdin <<EOF
BEGIN
{
	send = receive = 0;
}

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[4]->ipv4_protocol == IPPROTO_UDP/
{
	send++;
}

ip:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[4]->ipv4_protocol == IPPROTO_UDP/
{
	receive++;
}

END
{
	printf("Minimum UDP events seen\n\n");
	printf("ip:::send - %s\n", send >= 1 ? "yes" : "no");
	printf("ip:::receive - %s\n", receive >= 1 ? "yes" : "no");
}
EOF
