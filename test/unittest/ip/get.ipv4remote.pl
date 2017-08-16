#!/usr/bin/perl -w
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
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
