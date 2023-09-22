#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2023, Oracle and/or its affiliates. All rights reserved.
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

# Eliminate /usr/sbin and /usr/bin from PATH so that we do not find tshark.
# We will fall back to tracemem()-like output.
PATH= $dtrace $dt_flags -x pcapsize=$pcapsize -w -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE 1> $file

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	printf("pcap output:\n");
	pcap((struct sk_buff *)arg0, PCAP_IP);
	printf("tracemem output:\n");
	tracemem(((struct sk_buff *)arg0)->data, $pcapsize);
}
EODTRACE

if [ $? -ne 0 ]; then
	echo DTrace failed
	exit 1
fi

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
				echo "mismatch:"
				echo "  pcap    : '$pcapline'"
				echo "  tracemem: '$line'"
				echo
				echo "file:"
				cat $file
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
