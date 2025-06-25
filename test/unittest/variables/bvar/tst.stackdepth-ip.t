#!/bin/sh

testdir="$(dirname $_test)"
local=127.0.0.1

$testdir/../../ip/perlping.pl icmp $local
