#!/usr/bin/perl -w

#
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# server.udp.pl [localHost] [localPort]
#
# Open udp port echoing messages back.
#

use strict;
use IO::Socket::IP;

my $localAddr = $ARGV[0];
my $localPort = $ARGV[1];

my $s = IO::Socket::IP->new(
	Proto => 'udp',
	LocalHost => $localAddr,
	LocalPort => $localPort);
die "Could not create socket for port $localPort" unless $s;

my $MAXLEN = 20;
my $msg;
while ($s->recv($msg, $MAXLEN)) {
	$s->send($msg);
}
close($s);
sleep 5;
