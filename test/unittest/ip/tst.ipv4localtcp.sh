#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright © 2008, 2017, Oracle and/or its affiliates. All rights reserved.
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
# 2. The loopback interface missing or not up.
# 3. The local ssh service is not online.
# 4. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test performs a TCP connection and checks that at least the
# following packet counts were traced:
#
# 3 x ip:::send (2 during the TCP handshake, then a FIN)
# 2 x ip:::receive (1 during the TCP handshake, then the FIN ACK)
#
# The network stack may elide at least one of these, so check for 4.
#

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
local=127.0.0.1
tcpport=22

cat > $tmpdir/tst.ipv4localtcp.test.pl <<-EOPERL
	use IO::Socket;
	my \$s = IO::Socket::INET->new(
	    Proto => "tcp",
	    PeerAddr => "$local",
	    PeerPort => $tcpport,
	    Timeout => 3);
	die "Could not connect to host $local port $tcpport" unless \$s;
	close \$s;
EOPERL

$dtrace $dt_flags -c "/usr/bin/perl $tmpdir/tst.ipv4localtcp.test.pl" -qs /dev/stdin <<EODTRACE
BEGIN
{
	send = receive = 0;
}

ip:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[4]->ipv4_protocol == IPPROTO_TCP/
{
	send++;
}

ip:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
    args[4]->ipv4_protocol == IPPROTO_TCP/
{
	receive++;
}

END
{
	printf("Minimum TCP events seen\n\n");
	printf("ip:::send - %s\n", send >= 4 ? "yes" : "no");
	printf("ip:::receive - %s\n", receive >= 4 ? "yes" : "no");
}
EODTRACE

status=$?

exit $status
