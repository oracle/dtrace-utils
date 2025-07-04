#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test ip:::{send,receive} of IPv4 TCP to a remote host.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test performs a TCP connection and checks that at least the
# following packet counts were traced:
#
# 3 x ip:::send (2 during the TCP handshake, then a FIN)
# 2 x ip:::receive (1 during the TCP handshake, then the FIN ACK)
# 

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
getaddr=$testdir/../../utils/get_remote.sh
tcpport="22"
dest=""

if [[ ! -x $getaddr ]]; then
	echo "could not find or execute sub program: $getaddr" >&2
	exit 3
fi

set -- $($getaddr ipv4 $tcpport)
source="$1"
dest="$2"

if [[ $? -ne 0 ]] || [[ -z $dest ]]; then
	exit 67
fi

cat > $tmpdir/tst.ipv4remotetcp.test.pl <<-EOPERL
	use IO::Socket;
	for (my \$i = 0; \$i < 3; \$i++) {
		my \$s = IO::Socket::INET->new(
		    Proto => "tcp",
		    PeerAddr => "$dest",
		    PeerPort => $tcpport,
		    Timeout => 3);
		die "Could not connect to host $dest port $tcpport" unless \$s;
		close \$s;
	}
EOPERL

$dtrace $dt_flags -c "/usr/bin/perl $tmpdir/tst.ipv4remotetcp.test.pl" -qs /dev/stdin <<EODTRACE
BEGIN
{
	send = receive = 0;
}

ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->ipv4_protocol == IPPROTO_TCP/
{
	send++;
}

ip:::receive
/args[2]->ip_saddr == "$dest" && args[2]->ip_daddr == "$source" &&
    args[4]->ipv4_protocol == IPPROTO_TCP/
{
	receive++;
}
END
{
	printf("Minimum TCP events seen\n\n");
	printf("ip:::send - %s\n", send >= 3 ? "yes" : "no");
	printf("ip:::receive - %s\n", receive >= 2 ? "yes" : "no");
}
EODTRACE

status=$?

exit ${status}
