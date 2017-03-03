#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2008, 2017 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
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

#
# Shake loose any ICMPv6 Neighbor advertisement messages before tracing.
#
/usr/bin/ping6 -c 3 $dest > /dev/null 2>&1

$dtrace $dt_flags -c "/bin/ping6 -c 3 $dest" -qs /dev/stdin <<EOF | \
    grep -v 'is alive' | sort -n
ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[5]->ipv6_nexthdr == IPPROTO_ICMPV6/
{
	printf("1 ip:::send    (");
	printf("args[2]: %d %d, ", args[2]->ip_ver, args[2]->ip_plength);
	printf("args[5]: %d %d %d)\n",
	    args[5]->ipv6_ver, args[5]->ipv6_tclass, args[5]->ipv6_plen);
}

ip:::receive
/args[2]->ip_saddr == "$dest" && args[2]->ip_daddr == "$source" &&
    args[5]->ipv6_nexthdr == IPPROTO_ICMPV6/
{
	printf("2 ip:::receive (");
	printf("args[2]: %d, ", args[2]->ip_ver);
	printf("args[5]: %d)\n", args[5]->ipv6_ver);
}
EOF
