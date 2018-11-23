#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Ensure pcap() action directed at stdout matches tracemem output of
# skb->data.
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

# By eliminating /usr/sbin from PATH we ensure we cannot find tshark
# so fall back to tracemem()-like output.
PATH=/usr/bin $dtrace $dt_flags -x pcapsize=$pcapsize -w -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE 1> $file

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	pcap((struct sk_buff *)arg0, PCAP_IP);
	tracemem(((struct sk_buff *)arg0)->data, $pcapsize);
}
EODTRACE

if [[ -f $file ]]; then
	pcapline=""
	while read line ; do
		# Skip non-hexdump lines...
		if ! [[ $line =~ "0:" ]]; then
			continue
		fi
		if [[ -z $pcapline ]]; then
			# Record pcap hexdump line to match tracemem line.
			pcapline=$line
		else
			# Finally, compare pcap line to tracemem line.
			if [[ "$pcapline" != "$line" ]]; then
				echo "'$pcapline' and '$line' did not match"
				exit 1
			fi
			pcapline=""
		fi
	done < $file
	rm -f $file
	exit 0
else
	echo "No such file $file"
	exit 1
fi
