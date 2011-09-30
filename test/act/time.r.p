#!/bin/gawk -f
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright 2011 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.

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
