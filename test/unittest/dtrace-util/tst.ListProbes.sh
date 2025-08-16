#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
# ASSERTION:
# Testing -l option gives reasonable numbers of probes for each provider.
#
# SECTION: dtrace Utility/-ln Option
##

dtrace=$1

# Decide whether to expect lockstat (disabled prior to 5.10).
read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`
if [ $MAJOR -gt 5 ]; then
        expect_lockstat=1
elif [ $MAJOR -eq 5 -a $MINOR -ge 10 ]; then
        expect_lockstat=1
else
        expect_lockstat=0
fi

# Run "dtrace -l", print the providers, aggregate by provider, then confirm counts.
$dtrace $dt_flags -l \
| gawk '{ print $2 }' \
| sort | uniq -c \
| gawk -v LCKSTT=$expect_lockstat '

    BEGIN {
        nerr = 0;

        # Fake lockstat if not expected.
        if (LCKSTT == 0) print "lockstat";
    }

    function mycheck(lbl, val, valmin, valmax) {
        print lbl;
        if (val < valmin) { print "ERROR:", lbl, val, "< MIN =", valmin; nerr++ }
        if (val > valmax) { print "ERROR:", lbl, val, "> MAX =", valmax; nerr++ }
    }

    # Skip the banner.
    /^ +1 PROVIDER$/ { next }

    # Wrong number of fields.
    NF != 2 {
        print "ERROR: wrong number of fields", $0;
        nerr++;
        next;
    }

    # Recognize some providers; apply sanity check on number of probes.
    $2 == "cpc"      { mycheck($2, $1,     5,    500); next }
    $2 == "dtrace"   { mycheck($2, $1,     3,      3); next }
    $2 == "fbt"      { mycheck($2, $1, 30000, 300000); next }
    $2 == "io"       { mycheck($2, $1,     2,     20); next }
    $2 == "ip"       { mycheck($2, $1,     2,     20); next }
    $2 == "lockstat" { mycheck($2, $1,     4,     40); next }
    # nothing for pid
    $2 == "proc"     { mycheck($2, $1,     6,     30); next }
    $2 == "profile"  { mycheck($2, $1,     6,     30); next }
    $2 == "rawfbt"   { mycheck($2, $1, 30000, 300000); next }
    $2 == "rawtp"    { mycheck($2, $1,   600,   6000); next }
    $2 == "sched"    { mycheck($2, $1,     3,     30); next }
    $2 == "sdt"      { mycheck($2, $1,   600,   6000); next }
    $2 == "syscall"  { mycheck($2, $1,   300,   3000); next }
    $2 == "tcp"      { mycheck($2, $1,     8,      8); next }
    $2 == "udp"      { mycheck($2, $1,     2,      2); next }	
    # nothing for usdt

    # Unrecognized line.
    {
        print "ERROR: unrecognized line", $0;
        nerr++;
    }

    END { exit(nerr == 0 ? 0 : 1) }'

exit $?
