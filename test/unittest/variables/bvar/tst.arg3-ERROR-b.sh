#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
# ASSERTION: The ERROR arg3 value reports the correct PC value.
#
# SECTION: Variables/Built-in Variables/arg3
##

dtrace=$1
tmpdir=${tmpdir:-/tmp}

DIRNAME="$tmpdir/arg3-ERROR-b.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

$dtrace $dt_flags -xdisasm=8 -Sqs /dev/stdin << EOT &> D.out
BEGIN
{
	x = (int *)64;
	y = *x;                       /* 1st ERROR */
}

BEGIN
{
	x = (int *)64;
	y = *x;                       /* 2nd ERROR */
}

BEGIN
{
	x = (int *)64;
	y = *x;                       /* 3rd ERROR */
}

BEGIN
{
	x = (int *)64;
	y = *x;                       /* 4th ERROR */
}

BEGIN
{
	exit(0);
}
EOT

gawk 'BEGIN {
	ok = 0;
     }

     /call dt_probe_error/ {
	sites[int($1)] = 1;
	next;
     }

     /error in dt_clause_/ {
	if (!($NF in sites)) {
	    print;
	    print "  No call to dt_probe_error found at PC " $NF;
	} else {
	    ok++;
	    delete sites[$NF];
	}
     }

     END {
	if (ok != 4) {
	    print "\nFound " ok " valid calls to dt_probe_error, expected 4";
	    exit 1;
	}

	print "OK";
	exit 0;
     }' D.out

rc=$?

[ $rc -ne 0 ] && cat D.out

rm -rf $DIRNAME

exit $rc
