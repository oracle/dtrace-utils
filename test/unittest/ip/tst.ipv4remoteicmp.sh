#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv4 ICMP to a remote host.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. No physical network interface is plumbed and up.
# 3. The subnet gateway is not reachable.
# 4. An unrelated ICMP between these hosts was traced by accident.
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
$dtrace $dt_flags -c "$testdir/perlping.pl icmp $dest" -qs /dev/stdin <<EOF | \
	sort -n
ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->ipv4_protocol == IPPROTO_ICMP/
{
	printf("1 ip:::send    (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[4]: %d %d %d %d %d)\n",
	    args[4]->ipv4_ver, args[4]->ipv4_length, args[4]->ipv4_flags,
	    args[4]->ipv4_offset, args[4]->ipv4_ttl);
}

ip:::receive
/args[2]->ip_saddr == "$dest" && args[2]->ip_daddr == "$source" &&
    args[4]->ipv4_protocol == IPPROTO_ICMP/
{
	printf("2 ip:::receive (");
	printf("args[2]: %d, ", args[2]->ip_ver);
	printf("args[4]: %d)\n", args[4]->ipv4_ver);
}
EOF
