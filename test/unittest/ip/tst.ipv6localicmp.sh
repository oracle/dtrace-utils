#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv6 ICMP to a local address.  This creates a
# temporary lo0/inet6 interface if one doesn't already exist.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. Unrelated ICMPv6 on lo traced by accident.
#

# possible paths for ping6
export PATH=/bin:/usr/bin:/sbin:/usr/sbin:$PATH

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
local=::1

# Check that we have an IPv6 local configuration to work with, or expect fail
/sbin/ip -o route get to $local > /dev/null || exit 67

$dtrace $dt_flags -c "ping6 -q $local -c 3" -qs /dev/stdin <<EOF | \
    awk '/ip::/ { print $0 }' | sort -n
ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[5]->ipv6_nexthdr == IPPROTO_ICMPV6/
{
	printf("1 ip:::send    (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[5]: %d %d %d)\n",
	    args[5]->ipv6_ver, args[5]->ipv6_tclass, args[5]->ipv6_plen);
}

ip:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[5]->ipv6_nexthdr == IPPROTO_ICMPV6/
{
	printf("2 ip:::receive (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[5]: %d %d %d)\n",
	    args[5]->ipv6_ver, args[5]->ipv6_tclass, args[5]->ipv6_plen);
}
EOF
