#!/bin/bash

#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# Test {ip,udp}:::{send,receive} of IPv4 UDP to a remote host.
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
# 1 x udp:::send (UDP sent)
#

# @@skip: not sure what port 31337 is supposed to be

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
getaddr=$testdir/../ip/get.ipv4remote.pl
port=31337

if [[ ! -x $getaddr ]]; then
	echo "could not find or execute sub program: $getaddr" >&2
	exit 3
fi
read source dest <<<`$getaddr 2>/dev/null`
if (( $? != 0 )) || [[ -z $dest ]]; then
	exit 67
fi

$dtrace $dt_flags -c "$testdir/../ip/client.ip.pl udp $dest $port" -qs /dev/stdin <<EOF 2>/dev/null
BEGIN
{
	ipsend = udpsend = 0;
}

ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->ipv4_protocol == IPPROTO_UDP/
{
	ipsend++;
}

udp:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" /
{
	udpsend++;
}

END
{
	printf("Minimum UDP events seen\n\n");
	printf("ip:::send - %s\n", ipsend >= 1 ? "yes" : "no");
	printf("udp:::send - %s\n", udpsend >= 1 ? "yes" : "no");
}
EOF
