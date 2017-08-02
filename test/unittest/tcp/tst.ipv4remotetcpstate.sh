#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright © 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Test tcp:::state-change and tcp:::{send,receive} by connecting to
# the remote tcp service and sending a test message. This should result
# in a "Protocol mismatch" response and a close of the connection.
# A number of state transition events along with tcp send and receive
# events for the message should result.
#
# This may fail due to:
#
# 1. A change to the ip stack breaking expected probe behavior,
#    which is the reason we are testing.
# 2. The remote ssh service is not online.
# 3. An unlikely race causes the unlocked global send/receive
#    variables to be corrupted.
#
# This test performs a TCP connection to the ssh service (port 22) and
# checks that at least the following packet counts were traced:
#
# 4 x ip:::send (2 during the TCP handshake, the message, then a FIN)
# 4 x tcp:::send (2 during the TCP handshake, the messages, then a FIN)
# 3 x ip:::receive (1 during the TCP handshake, the response, then the FIN ACK)
# 3 x tcp:::receive (1 during the TCP handshake, the response, then the FIN ACK)
#
# For this test to work, we are assuming that the TCP handshake and
# TCP close will enter the IP code path and not use tcp fusion.
#

if (( $# != 1 )); then
	print -u2 "expected one argument: <dtrace-path>"
	exit 2
fi

dtrace=$1
testdir="$(dirname $_test)"
getaddr=$testdir/../ip/get.ipv4remote.pl
client=$testdir/../ip/client.ip.pl
tcpports="22 80"
tcpport=""
dest=""

if [[ ! -x $getaddr ]]; then
	echo "could not find or execute sub program: $getaddr" >&2
	exit 3
fi
for port in $tcpports ; do
	res=`$getaddr $port 2>/dev/null`
	if (( $? == 0 )); then
		read s d <<< $res
		tcpport=$port
		source=$s
		dest=$d
		break
	fi
done

if [ -z $tcpport ]; then
        exit 67
fi


$dtrace $dt_flags -c "$testdir/../ip/client.ip.pl tcp $dest $tcpport" -qs /dev/stdin <<EODTRACE
BEGIN
{
	ipsend = tcpsend = ipreceive = tcpreceive = 0;
	connreq = connest = 0;
}

ip:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->ipv4_protocol == IPPROTO_TCP/
{
	ipsend++;
}

tcp:::send
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
    args[4]->tcp_dport == $tcpport/
{
	tcpsend++;
}

ip:::receive
/args[2]->ip_saddr == "$dest" && args[2]->ip_daddr == "$source" &&
    args[4]->ipv4_protocol == IPPROTO_TCP/
{
	ipreceive++;
}

tcp:::receive
/args[2]->ip_saddr == "$dest" && args[2]->ip_daddr == "$source" &&
    args[4]->tcp_sport == $tcpport/
{
	tcpreceive++;
}

tcp:::state-change
{
	state_event[args[3]->tcps_state]++;
}

tcp:::connect-request
/args[2]->ip_saddr == "$source" && args[2]->ip_daddr == "$dest" &&
 args[4]->tcp_dport == $tcpport/
{
	connreq++;
}

tcp:::connect-established
/args[2]->ip_saddr == "$dest" && args[2]->ip_daddr == "$source" &&
 args[4]->tcp_sport == $tcpport/
{
	connest++;
}

END
{
	printf("Minimum TCP events seen\n\n");
	printf("ip:::send - %s\n", ipsend >= 4 ? "yes" : "no");
	printf("ip:::receive - %s\n", ipreceive >= 3 ? "yes" : "no");
	printf("tcp:::send - %s\n", tcpsend >= 4 ? "yes" : "no");
	printf("tcp:::receive - %s\n", tcpreceive >= 3 ? "yes" : "no");
	printf("tcp:::state-change to syn-sent - %s\n",
	    state_event[TCP_STATE_SYN_SENT] >=1 ? "yes" : "no");
	printf("tcp:::state-change to established - %s\n",
	    state_event[TCP_STATE_ESTABLISHED] >= 1 ? "yes" : "no");
	printf("tcp:::state-change to fin-wait-1 - %s\n",
	    state_event[TCP_STATE_FIN_WAIT_1] >= 1 ? "yes" : "no");
	/* If we are using port 80 many transition from closing -> time-wait */
	printf("tcp:::state-change to fin-wait-2 - %s\n",
	    state_event[TCP_STATE_FIN_WAIT_2] >= 0 ? "yes" : "no");
	printf("tcp:::state-change to time-wait - %s\n",
	    state_event[TCP_STATE_TIME_WAIT] >= 1 ? "yes" : "no");
	printf("tcp:::connect-request - %s\n",
	    connreq >=1 ? "yes" : "no");
	printf("tcp:::connect-established - %s\n",
	    connest >=1 ? "yes" : "no");
}
EODTRACE
