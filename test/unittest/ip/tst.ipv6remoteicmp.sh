#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv6 ICMP to a remote host.  This test is
# skipped if there are no physical interfaces configured with IPv6, or no
# other IPv6 hosts are reachable.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. An unrelated ICMPv6 between these hosts was traced by accident.
#

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
getaddr=$testdir/get.ipv6remote.pl

if [[ ! -x $getaddr ]]; then
	echo "could not find or execute sub program: $getaddr" >&2
	exit 3
fi
set -- $($getaddr)
source="$1"
dest="$2"
if [[ $? -ne 0 ]] || [[ -z $dest ]]; then
	echo -n "Could not find a local IPv6 interface and a remote IPv6 " >&2
	echo "host.  Aborting test." >&2
	exit 67
fi

nolinkdest="$(printf "%s" "$dest" | sed 's,%.*,,')"

$dtrace $dt_flags -c "/bin/ping6 -c 6 $dest" -qs /dev/stdin <<EOF | \
    awk '/ip:::/ { print $0 }' | sort -n
/* 
 * We use a size match to include only things that are big enough to
 * be pings, rather than neighbor solicitations/advertisements.
 */

ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$nolinkdest" &&
    args[5]->ipv6_nexthdr == IPPROTO_ICMPV6 && args[2]->ip_plength > 32/
{
	printf("1 ip:::send    (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[5]: %d %d %d)\n",
	    args[5]->ipv6_ver, args[5]->ipv6_tclass, args[5]->ipv6_plen);
}

ip:::receive
/args[2]->ip_saddr == "$nolinkdest" && args[2]->ip_daddr == "$source" &&
    args[5]->ipv6_nexthdr == IPPROTO_ICMPV6 && args[2]->ip_plength > 32/
{
	printf("2 ip:::receive (");
	printf("args[2]: %d, ", args[2]->ip_ver);
	printf("args[5]: %d)\n", args[5]->ipv6_ver);
}
EOF
