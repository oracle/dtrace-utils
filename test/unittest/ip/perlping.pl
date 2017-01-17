#!/usr/bin/perl -w
use Net::Ping;
my $p = Net::Ping->new("udp");
$p->ping(${ARGV[0]});
