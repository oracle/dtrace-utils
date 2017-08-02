#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2008, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv4 ICMP to a local address.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. The loopback interface missing or not up.
# 3. Unrelated ICMP on loopback traced by accident.
#

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=127.0.0.1

$dtrace $dt_flags -c "$testdir/perlping.pl icmp $local" -qs /dev/stdin <<EOF | sort -n
ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[4]->ipv4_protocol == IPPROTO_ICMP/
{
	printf("1 ip:::send    (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[4]: %d %d %d %d %d)\n",
	    args[4]->ipv4_ver, args[4]->ipv4_length, args[4]->ipv4_flags,
	    args[4]->ipv4_offset, args[4]->ipv4_ttl);
}

ip:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[4]->ipv4_protocol == IPPROTO_ICMP/
{
	printf("2 ip:::receive (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[4]: %d %d %d %d %d)\n",
	    args[4]->ipv4_ver, args[4]->ipv4_length, args[4]->ipv4_flags,
	    args[4]->ipv4_offset, args[4]->ipv4_ttl);
}
EOF
