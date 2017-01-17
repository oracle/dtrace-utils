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
# get.ipv4remote.pl [tcpport]
#
# Find an IPv4 reachable remote host using both ip(8) and ping(8).
# If a tcpport is specified, return a host that is also listening on this
# TCP port.  Print the local address and the remote address, or an
# error message if no suitable remote host was found.  Exit status is 0 if
# a host was found.  (Note: the only host we check is the gateway.  Nobody
# responds to broadcast pings these days, and portscanning the local net is
# unfriendly.)
#

use strict;
use IO::Socket;

my $TIMEOUT = 3;
my $tcpport = @ARGV == 1 ? $ARGV[0] : 0;

#
# Determine gateway IP address
#

my $local = "";
my $remote = "";
my $responsive = "";
my $up;
open IP, '/sbin/ip -o -4 route show |' or die "Couldn't run ip route show: $!\n";
while (<IP>) {
	next unless /^default /;

	if (/via (\S+)/) {
		$remote = $1;
	}
}
close IP;
die "Could not determine gateway router IP address" if $remote eq "";

open IP, "/sbin/ip -o route get to $remote |" or die "Couldn't run ip route get: $!\n";
while (<IP>) {
	next unless /^$remote /;
	if (/src (\S+)/) {
		$local = $1;
	}
}
close IP;
die "Could not determine local IP address" if $local eq "";

#
# See if the rmote host responds to an icmp echo.
#
open PING, "/bin/ping -n -s 56 -w $TIMEOUT $remote |" or
    die "Couldn't run ping: $!\n";
while (<PING>) {
	if (/bytes from (.*): /) {
		my $addr = $1;

		if ($tcpport != 0) {
			#
			# Test TCP
			#
			my $socket = IO::Socket::INET->new(
				Proto    => "tcp",
				PeerAddr => $addr,
				PeerPort => $tcpport,
				Timeout  => $TIMEOUT,
			);
			next unless $socket;
			close $socket;
		}

		$responsive = $addr;
		last;
	}
}
close PING;
die "Can't find a remote host for testing: No suitable response from " .
    "$remote\n" if $responsive eq "";

print "$local $responsive\n";
