#!/usr/bin/perl -w
# Oracle Linux DTrace.
# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
use Net::Ping;
my $p = Net::Ping->new(${ARGV[0]}, 5, 56);
$p->ping(${ARGV[1]});
