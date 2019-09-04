#!/usr/bin/gawk -f
#
# Oracle Linux DTrace.
# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# If both the walltime and date stamps show the same day, we allow
# them by converting both to the string @@correct-date@@. This has a race window if
# run across midnight, but that is really not important.

BEGIN {
    seen_walltime=0
    seen_date=0
    seen_stderr=0;
    FS = " *: | *|:"
}

/-- @@stderr --/ {
    seen_stderr=1;
}

{
    if (seen_stderr == 1) {
        print $0
        next
    }
}

/walltime/ {
    # Two walltime lines with no intervening date: bug, print both unchanged.
    if ((seen_walltime) && (!seen_date)) {
        print complete_walltime
        print $0
        next
    }

    # Capture the date
    walltime_year=$2
    walltime_month=$3
    walltime_day=$4
    complete_walltime=$0
    seen_walltime = 1
    seen_date = 0
}

/date/ {
    # A date without a matching walltime: print it
    if (!seen_walltime) {
        print $0
        next
    }

    if ((walltime_year == $2) &&
        (walltime_month == $3) &&
        (walltime_day == $4)) {
        print "walltime  : @@correct-date@@"
        print "date      : @@correct-date@@"
    } else {
        print complete_walltime
        print $0
    }
    seen_walltime = 0
    seen_date = 1
}
