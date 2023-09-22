#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Ensure that when tshark initialization fails we fall back to tracemem(),
# and that pcap() action directed at stdout matches tracemem output of
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

# Force a failure to find tshark by jamming a nonexecutable 'tshark' in at the
# front of the path.
mkdir $tmpdir/failshark
cat > $tmpdir/failshark/tshark <<'EOF'
#!/bin/sh
exit 1
EOF
chmod a+x $tmpdir/failshark/tshark
PATH=$tmpdir/failshark:$PATH $dtrace $dt_flags -x pcapsize=$pcapsize -w -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE 1> $file

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	pcap((struct sk_buff *)arg0, PCAP_IP);
	tracemem(((struct sk_buff *)arg0)->data, $pcapsize);
}
EODTRACE

if [ $? -ne 0 ]; then
	echo DTrace failed
	exit 1
fi

rm -rf $tmpdir/failshark

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
