#!/bin/bash

#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# Test tcp:::state-change and tcp:::{send,receive} by connecting to
# the local ssh service and sending a test message. This should result
# in a "Protocol mismatch" response and a close of the connection.
# A number of state transition events along with tcp send and
# receive events for the message should result.
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
# This test performs a TCP connection to the ssh service (port 22) and
# checks that at least the following packet counts were traced:
#
#

if (( $# != 1 )); then
	print -u2 "expected one argument: <dtrace-path>"
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
local=127.0.0.1
tcpport=22

$dtrace $dt_flags -c "$testdir/../ip/client.ip.pl tcp $local $tcpport" -qs /dev/stdin <<EODTRACE
BEGIN
{
	tcpsend = tcpreceive = 0;
	connreq = connest = connaccept = 0;
}

tcp:::send
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 (args[4]->tcp_sport == $tcpport || args[4]->tcp_dport == $tcpport)/
{
	tcpsend++;
}

tcp:::receive
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 (args[4]->tcp_sport == $tcpport || args[4]->tcp_dport == $tcpport)/
{
	tcpreceive++;
}

tcp:::state-change
/ args[3]->tcps_laddr == "$local" /
{
	state_event[args[3]->tcps_state]++;
}

tcp:::connect-request
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 args[4]->tcp_dport == $tcpport/
{
	connreq++;
}

tcp:::connect-established
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 args[4]->tcp_sport == $tcpport/
{
	connest++;
}

tcp:::accept-established
/args[2]->ip_saddr == "$local" && args[2]->ip_daddr == "$local" &&
 args[4]->tcp_dport == $tcpport/
{
	connaccept++;
}

END
{
	printf("Minimum TCP events seen\n\n");
	printf("tcp:::send - %s\n", tcpsend >= 7 ? "yes" : "no");
	printf("tcp:::receive - %s\n", tcpreceive >= 7 ? "yes" : "no");
	printf("tcp:::state-change to syn-sent - %s\n",
	    state_event[TCP_STATE_SYN_SENT] >=1 ? "yes" : "no");
	printf("tcp:::state-change to syn-received - %s\n",
	    state_event[TCP_STATE_SYN_RECEIVED] >=1 ? "yes" : "no");
	printf("tcp:::state-change to established - %s\n",
	    state_event[TCP_STATE_ESTABLISHED] >= 2 ? "yes" : "no");
	printf("tcp:::state-change to fin-wait-1 - %s\n",
	    state_event[TCP_STATE_FIN_WAIT_1] >= 1 ? "yes" : "no");
	printf("tcp:::state-change to close-wait - %s\n",
	    state_event[TCP_STATE_CLOSE_WAIT] >= 1 ? "yes" : "no");
	printf("tcp:::state-change to fin-wait-2 - %s\n",
	    state_event[TCP_STATE_FIN_WAIT_2] >= 1 ? "yes" : "no");
	printf("tcp:::state-change to last-ack - %s\n",
	    state_event[TCP_STATE_LAST_ACK] >= 1 ? "yes" : "no");
	printf("tcp:::state-change to time-wait - %s\n",
	    state_event[TCP_STATE_TIME_WAIT] >= 1 ? "yes" : "no");
	printf("tcp:::connect-request - %s\n",
	    connreq >=1 ? "yes" : "no");
	printf("tcp:::connect-established - %s\n",
	    connest >=1 ? "yes" : "no");
	printf("tcp:::accept-established - %s\n",
	    connaccept >=1 ? "yes" : "no");
}
EODTRACE
