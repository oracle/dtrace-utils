#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@nosort

# Use a shell file rather than .d .r and .r.p files since we do not
# want to send a large volume of output to the log file in the normal
# case that the test passes.

dtrace=$1
tmpfile=$tmpdir/tst.parser4.$$
chkfile=$tmpfile.check

# Run DTrace.

$dtrace $dt_flags -xtree=4 -e -n '
BEGIN
{
	x = 1234;
	printf("%d\n", x);
	z = 32 * (x + 18);
        @agg[x, z, "hello"] = llquantize(x, 2, 3, 5, 2);
	exit(0);
}
' > $tmpfile 2>&1
if [ $? -ne 0 ]; then
	echo ERROR running DTrace
	cat $tmpfile
	exit 1
fi

# Generate check file.

cat > $chkfile << EOF
      PDESC :::BEGIN [0]
    CTXATTR [u/u/c]
    ACTION
      D EXPRESSION attr=[s/s/c]
        OP2 = (type=<int> attr=[s/s/c] flags=SIGN,COOK,WRITE)
          VARIABLE (normally-assigned) x (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
          INT 0x4d2 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
      D PRODUCER attr=[s/s/c]
        FUNC printf (type=<void> attr=[s/s/c] flags=SIGN,COOK)
          STRING "%d\n" (type=<string> attr=[s/s/c] flags=COOK,REF,DPTR)
        ,
          VARIABLE (normally-assigned) x (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
      D EXPRESSION attr=[s/s/c]
        OP2 = (type=<int> attr=[s/s/c] flags=SIGN,COOK,WRITE)
          VARIABLE (normally-assigned) z (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
          OP2 * (type=<int> attr=[s/s/c] flags=SIGN,COOK)
            INT 0x20 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
            OP2 + (type=<int> attr=[s/s/c] flags=SIGN,COOK)
              VARIABLE (normally-assigned) x (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
              INT 0x12 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
      D EXPRESSION attr=[s/s/c]
        AGGREGATE @agg attr=[s/s/c] [
          VARIABLE (normally-assigned) x (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
        ,
          VARIABLE (normally-assigned) z (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
        ,
          STRING "hello" (type=<string> attr=[s/s/c] flags=COOK,REF,DPTR)
        ]
        =
          FUNC llquantize (type=<<DYN>> attr=[s/s/c] flags=SIGN,COOK,REF)
            VARIABLE (normally-assigned) x (type=<int> attr=[s/s/c] flags=SIGN,COOK,LVAL,WRITE)
          ,
            INT 0x2 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
          ,
            INT 0x3 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
          ,
            INT 0x5 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
          ,
            INT 0x2 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
      D PRODUCER attr=[s/s/c]
        FUNC exit (type=<void> attr=[s/s/c] flags=SIGN,COOK)
          INT 0x0 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
EOF

# Check results.

gawk '
# Look for the BEGIN clause.
/PDESC :::BEGIN / {
        # Print until we get the next clause.
        while (index($0, "Parse tree") == 0) {
                print;
                getline;
        }
}' $tmpfile | diff -q - $chkfile > /dev/null
if [ $? -ne 0 ]; then
	echo ERROR in output
	cat $tmpfile
	exit 1
fi

exit 0
