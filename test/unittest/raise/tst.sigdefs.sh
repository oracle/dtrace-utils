#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Test the signal definitions that should be supplied by signal.d.
#

dtrace=$1
tmpfile=$tmpdir/tst.sigdefs.$$.d

cat << EOF > $tmpfile
BEGIN
{
    nerr = 0;
EOF

# before glibc 2.26, there was no signum-generic.h, but that's okay
cat /usr/include/bits/signum-generic.h /usr/include/bits/signum.h \
| awk '
    /SIGRTMIN/ || /SIGRTMAX/ || /SIGSTKSZ/ { next }
    /^#define[[:blank:]]*SIG[[:alnum:]]/ { signum[$2] = $3 }
    END {
            for (sig in signum) {
                num = signum[sig];
                if (match(num, "SIG") != 0) { num = signum[num] };
                printf("    nerr += %-10s != %2d ? 1 : 0;\n", sig, num);
            }
        }' >> $tmpfile

cat << EOF >> $tmpfile
}
BEGIN
/nerr/
{
    exit(1);
}
BEGIN
{
    exit(0);
}
EOF

$dtrace $dt_flags -qs $tmpfile
if [ $? -ne 0 ]; then
	echo D script failed:
	cat $tmpfile
	rm -f $tmpfile
	exit 1
fi

rm -f $tmpfile
exit 0

