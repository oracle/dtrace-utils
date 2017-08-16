#!/usr/bin/perl -w
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# client.ip.pl [proto] [peer address] [port]
#
# Connect to remote port for specified protocol/peer address.
#

use strict;
use IO::Socket::IP;

my $proto = $ARGV[0];
my $peerAddr = $ARGV[1];
my $peerPort = $ARGV[2];

my $s = IO::Socket::IP->new(
	Proto => $proto,
	PeerAddr => $peerAddr,
	PeerPort => $peerPort,
	Timeout => 10);
die "Could not connect to host $peerAddr port $peerPort" unless $s;

$s->setsockopt(SOL_SOCKET, SO_RCVTIMEO, pack('l!l!', 10, 0))
	or die "setsockopt: $!";
my $msg = 'hello world';
my $size = $s->send($msg);
$s->recv($msg, 1000);
close($s);
sleep 5;
