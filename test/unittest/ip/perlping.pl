#!/usr/bin/perl -w
use Net::Ping;
my $p = Net::Ping->new(${ARGV[0]}, 5, 56);
$p->ping(${ARGV[1]});
