#!/bin/bash
# 
# Oracle Linux DTrace.
# Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# This script ensures that we can enable a probe which specifies a platform
# specific event.
#

# @@skip: must port from cpustat and decide what a platform-specific event now means

if [ $# != 1 ]; then
        print -u2 "expected one argument: <dtrace-path>"
        exit 2
fi

dtrace=$1

get_event()
{
        perl /dev/stdin /dev/stdout << EOF
        open CPUSTAT, '/usr/sbin/cpustat -h |'
            or die  "Couldn't run cpustat: \$!\n";
        while (<CPUSTAT>) {
                if (/(\s+)event\[*[0-9]-*[0-9]*\]*:/ && !/PAPI/) {
                        @a = split(/ /, \$_);
                        \$event = \$a[\$#a-1];
                }
        }

        close CPUSTAT;
        print "\$event\n";
EOF
}

script()
{
        $dtrace $dt_flags -s /dev/stdin << EOD
        #pragma D option quiet
        #pragma D option bufsize=128k

        cpc:::$1-all-10000
        {
                @[probename] = count();
        }

        tick-1s
        /n++ > 5/
        {
                exit(0);
        }
EOD
}

event=$(get_event)
script $event

status=$?

exit $status
