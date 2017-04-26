#!/bin/bash

#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# Test ip:::{send,receive} of IPv6 UDP to a local address.
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
# This test sends a UDP message using perl and checks that at least the
# following counts were traced:
#
# 1 x ip:::send (UDP sent)
# 1 x ip:::receive (UDP received)
#
# Ensure timeout (which would kill client) does not leave server.udp.pl
# hanging around; allow plenty of time for test to run.
#
# @@timeout: 500

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=::1
udpport=52462

$testdir/server.udp.pl $local $udpport >/dev/null&
server=$!
disown %+
$dtrace $dt_flags -c "$testdir/../ip/client.ip.pl udp $local $udpport" -qs /dev/stdin <<EOF 2>/dev/null
BEGIN
{
	ipsend = ipreceive = udpsend = udpreceive = 0;
}

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[5]->ipv6_nexthdr == IPPROTO_UDP/
{
	ipsend++;
}

ip:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[5]->ipv6_nexthdr == IPPROTO_UDP/
{
	ipreceive++;
}

udp:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 args[4]->udp_dport == $udpport /
{
	udpsend++;
}

udp:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 args[4]->udp_dport == $udpport /
{
	udpreceive++;
}

END
{
	printf("Minimum UDP events seen\n\n");
	printf("ip:::send - %s\n", ipsend >= 1 ? "yes" : "no");
	printf("ip:::receive - %s\n", ipreceive >= 1 ? "yes" : "no");
	printf("udp:::send - %s\n", udpsend >= 1 ? "yes" : "no");
	printf("udp:::receive - %s\n", udpreceive >= 1 ? "yes" : "no");
}
EOF

kill -TERM $server
