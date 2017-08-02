#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv4 UDP to a remote host.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. No physical network interface is plumbed and up.
# 3. The gateway is not reachable and listening on rpcbind.
# 4. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test sends a UDP message using ping and checks that at least the
# following counts were traced:
#
# 1 x ip:::send (UDP sent to ping's base UDP port)
# 

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
getaddr=$testdir/get.ipv4remote.pl

if [[ ! -x $getaddr ]]; then
	echo "could not find or execute sub program: $getaddr" >&2
	exit 3
fi
set -- $($getaddr)
source="$1"
dest="$2"
if [[ $? -ne 0 ]] || [[ -z $dest ]]; then
	exit 67
fi

$dtrace $dt_flags -c "$testdir/perlping.pl udp $dest" -qs /dev/stdin <<EOF | grep -v 'is alive'
BEGIN
{
	send = 0;
}

ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->ipv4_protocol == IPPROTO_UDP/
{
	send++;
}

END
{
	printf("Minimum UDP events seen\n\n");
	printf("ip:::send - %s\n", send >= 1 ? "yes" : "no");
}
EOF
