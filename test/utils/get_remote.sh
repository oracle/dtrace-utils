#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# get_remote.sh ipv4|ipv6|cleanup [tcpport]
#
# Create (or cleanup) a network namespace with either IPv4 or IPv6
# address associated.
#
# Print the local address and the remote address, or an
# error message if a failure occurred during setup.
#
# If tcpport is specified, start sshd on that port.
#
# Exit status is 0 if all succceeded.
#

cmd=$1
tcpport=$2

prefix=$(basename $tmpdir)
netns=${prefix}ns
veth1=${prefix}v1
veth2=${prefix}v2
mtu=1500

set -e

case $cmd in
cleanup)	pids=$(ip netns pids ${netns} 2>/dev/null)
		if [[ -n "$pids" ]]; then
			kill -TERM $pids
		fi
		ip netns del ${netns} 2>/dev/null
		exit 0
		;;
 ipv4)		veth1_addr=192.168.168.1
		veth2_addr=192.168.168.2
		prefixlen=24
		family=
		;;
 ipv6)		veth1_addr=fd::1
		veth2_addr=fd::2
		prefixlen=64
		family=-6
		;;
 *)		echo "Unexpected cmd $cmd" >2
		exit 1
		;;
esac

ip netns add $netns
ip link add dev $veth1 mtu $mtu netns $netns type veth \
   peer name $veth2 mtu $mtu
ip netns exec $netns ip $family addr add ${veth1_addr}/$prefixlen dev $veth1
ip netns exec $netns ip link set $veth1 up
ip addr add ${veth2_addr}/${prefixlen} dev $veth2
ip link set $veth2 up

if [[ -n "$tcpport" ]]; then
	sshd=$(which sshd)
	ip netns exec $netns $sshd -p $tcpport &
fi

echo "$veth2_addr $veth1_addr"
exit 0
