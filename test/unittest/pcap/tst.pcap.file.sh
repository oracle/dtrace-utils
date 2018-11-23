#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Ensure pcap() action directed to file succeeds, and verify the content
# using tshark.
#

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=127.0.0.1
tcpport=22
pcapsize=20

file=$tmpdir/pcap.out.$$

$dtrace $dt_flags -x pcapsize=$pcapsize -w -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	freopen("$file");
	pcap((struct sk_buff *)arg0, PCAP_IP);
	freopen("");
}
EODTRACE

if [[ -f $file ]]; then
	tsharkout=`tshark -r $file \
	    -Y 'ip.src==127.0.0.1 and ip.dst==127.0.0.1'`
	if [[ -z "$tsharkout" ]]; then
		echo "Expected non-empty capture file"
		exit 1
	fi
	exit 0
else
	echo "No such file $file"
	exit 1
fi
