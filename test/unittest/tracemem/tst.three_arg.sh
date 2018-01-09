#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION:
# Compare tracemem() results for two- and three-argument forms.
#
# SECTION: Actions and Subroutines/tracemem()
#
# @@timeout: 60
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi
dtrace=$1

#
# 26223475 DTrace tracemem on Linux missing support for 3-argument form
#

DIRNAME="$tmpdir/three_arg_tracemem.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# find a CPU to use (use first one we have available)
cpu=`taskset -cp $$ | awk '{print $NF}' | tr ',' ' ' | tr '-' ' '  | awk '{print $1}'`

# trigger: copy bytes from /dev/zero to /dev/null
# bind to a CPU so DTrace output from different CPUs will not be interwoven
CMD="taskset -c $cpu dd iflag=count_bytes count=8GB if=/dev/zero of=/dev/null status=none"

# DTrace test
#   - tracemem(2 args) writes 8 bytes (to check the 3-arg data)
#   - tracemem(3 args) writes alternatively 5 or 3 bytes
$dtrace -q $dt_flags -c "$CMD" -o out.txt -s /dev/stdin <<EOF
BEGIN
{
  x = 3;
}
profile-3
/arg0 && pid==\$target/
{
  x = 8 - x;
  tracemem(arg0, 8);
  tracemem(arg0, 32, x);
}
EOF

# check result
status=$?
if [ "$status" -ne 0 ]; then
        echo $tst: dtrace failed
        rm -f out.txt
        exit $status
fi

# awk file to post-process the data
cat > out.awk <<EOF
BEGIN {
    state = 0;   # moves cyclically from 0 to 1 to 2 to 3 and back to 0
    nerrs = 0;   # number of errors
    nchks = 0;   # number of lines that are correct
}
{
    if ( and(state, 1) ) {       # state 1 and state 3
        n = 5;                   # number of bytes is 5 for state 1 and 3 for state 3
        if ( state == 3 ) {
            n = 3;
            state = -1;          # cycle state back around
        }
        if ( NF != n ) {
            printf "ERROR expected %d bytes but got %d\n", n, NF;
            nerrs += 1;
            exit 1;
        }
        for (i = 0; i < n; i++) {
            if ( x[i] != (\$i + 0) ) {    # $i+0 to convert $i to number
                printf "ERROR expected byte %x but got %x\n", x[i], \$i;
                nerrs += 1;
                exit 1;
            }
        }
        nchks += 1;
    } else {                             # state 0 and state 2
        if ( NF != 8 ) {                 # number of bytes is 8
            printf "ERROR expected 8 bytes but got %d\n", NF;
            nerrs += 1;
            exit 1;
        }
        for (i = 0; i < 8; i++) x[i] = (\$i + 0);  # $i+0 to convert $i to number
    }
    state += 1;
}
END {
    if ( nerrs == 0 ) {
        if ( nchks < 4 ) {
            printf "insufficient number (only %d) of checks\n", nchks;
        } else {
            print "SUCCESS";
        }
    }
}
EOF

# now post-process
#   - cut header lines
#   - cut empty lines
#   - cut encoded tail
#   - cut prefix
#   - do the awk processing
grep -v "0123456789abcdef" out.txt \
| grep -v '^$' \
| cut -c-55 \
| sed 's/^ *0://' \
| awk -f out.awk > out.post

if [ `grep -c SUCCESS out.post` -eq 0 ]; then
    echo $tst: TEST FAILED
    echo "==================== out.txt start"
    cat out.txt
    echo "==================== out.txt end"
    echo "==================== out.post start"
    cat out.post
    echo "==================== out.post end"
    status=1
fi

exit $status

