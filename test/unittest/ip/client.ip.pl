#!/usr/bin/perl -w
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2008, 2017 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
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
