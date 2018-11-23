#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Ensure pcap() action in absence of freopen() results in tshark output
# when tshark is available.
#

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=127.0.0.1
tcpport=22
pcapsize=16

file=$tmpdir/pcap.out.$$

$dtrace $dt_flags -x pcapsize=$pcapsize -w -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE 1> $file

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	pcap((struct sk_buff *)arg0, PCAP_IP);
}
EODTRACE

if [[ -f $file ]]; then
	while read line ; do
		if [[ $line =~ "$local" ]]; then
			rm -f $file
			exit 0
		fi
	done < $file
	rm -f $file
	echo "Did not find pcap output including $local"
	exit 1
else
	echo "No such file $file"
	exit 1
fi
