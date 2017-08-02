#!/bin/bash

#
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# Test tcp:::{send,receive} of IPv6 TCP to the local host.
#
# This may fail due to:
#
# 1. A change to the tcp stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. The lo interface missing or not up.
# 3. The local ssh service is not online.
# 4. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test performs a TCP connection and checks that at least the
# following packet counts were traced:
#
# 3 x tcp:::send (2 during the TCP handshake, then a FIN)
# 2 x tcp:::receive (1 during the TCP handshake, then the FIN ACK)
#
# The actual count tested is 5 each way, since we are tracing both
# source and destination events.
#
# For this test to work, we are assuming that the TCP handshake and
# TCP close will enter the IP code path and not use tcp fusion.
#

if (( $# != 1 )); then
	echo "expected one argument: <dtrace-path>" >&2
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=::1
tcpport=22

$dtrace $dt_flags -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE
BEGIN
{
	send = receive = 0;
}

tcp:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	send++;
}

tcp:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" /
{
	receive++;
}

END
{
	printf("Minimum TCP events seen\n\n");
	printf("tcp:::send - %s\n", send >= 5 ? "yes" : "no");
	printf("tcp:::receive - %s\n", receive >= 5 ? "yes" : "no");
}
EODTRACE
