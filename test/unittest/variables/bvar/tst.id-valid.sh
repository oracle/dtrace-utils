#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The id built-in variable matches the ID.
#
# SECTION: Variables/Built-in Variables/id
#
##

dtrace=$1

# Testing turns off default ID output.
# For this test, however, we want that ID output.
unset _DTRACE_TESTING

# Feed DTrace output into awk script to process results.
$dtrace $dt_flags -n '
profile-3 {
	trace(id);
}

tick-2 {
	exit(0);
}
' | awk '
BEGIN {
        nevents = 0;
        nerrors = 0;
        ID = -1;
}

{ print }

$3 == ":profile-3" {
        nevents++;
        if (NF != 4) { nerrors++; print; print "ERROR: not 4 fields" };
        if ($2 != $4) { nerrors++; print; print "ERROR: ID and id differ" };
        if (ID == -1) { ID = $2 };
        if (ID != $2) { nerrors++; print; print "ERROR: probe ID changed" };
}

END {
        if (nevents == 0) { print "NO EVENTS"; exit(1) };
        if (nerrors != 0) { print "FOUND ERRORS"; exit(1) };
        print "success";
        exit(0);
}
'

exit $?

