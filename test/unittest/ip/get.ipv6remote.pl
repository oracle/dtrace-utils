#!/usr/bin/perl -w
#
# Oracle Linux DTrace.
# Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# get.ipv6remote.pl
#
# Find an IPv6 reachable remote host using both ip(8) and ping(8).
# Print the local address and the remote address, or print nothing if either
# no IPv6 interfaces or remote hosts were found.  (Remote IPv6 testing is
# considered optional, and so not finding another IPv6 host is not an error
# state we need to log.)  Exit status is 0 if a host was found.
#

use strict;
use IO::Socket;

my $TIMEOUT = 3;			# connection timeout

# possible paths for ping6
$ENV{'PATH'} = "/bin:/usr/bin:/sbin:/usr/sbin:$ENV{'PATH'}";

#
# Determine local IP address
#
my $local = "";
my $remote = "";
my $responsive = "";
my $up;
open IP, '/sbin/ip -o -6 route show |' or die "Couldn't run ip route show: $!\n";
while (<IP>) {
	next unless /^default /;

	if (/via (\S+)/) {
		$remote = $1;
	}
}
close IP;
die "Could not determine gateway router IPv6 address" if $remote eq "";

open IP, "/sbin/ip -o route get to $remote |" or die "Couldn't run ip route get: $!\n";
while (<IP>) {
	next unless /^$remote /;
	if (/src (\S+)/) {
		$local = $1;
	}
}
close IP;
die "Could not determine local IPv6 address" if $local eq "";

#
# Find the first remote host that responds to an icmp echo,
# which isn't a local address.
#
open PING, "ping6 -n -s 56 -w $TIMEOUT $remote 2>/dev/null |" or
    die "Couldn't run ping: $!\n";
while (<PING>) {
	if (/bytes from (.*): /) {
		$responsive = $1;
		last;
	}
}
close PING;
exit 2 if $responsive eq "";

print "$local $responsive\n";
