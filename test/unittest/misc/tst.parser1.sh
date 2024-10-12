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
tmpfile=$tmpdir/tst.parser1.$$
chkfile=$tmpfile.check

# Run DTrace.

$dtrace -xtree=1 -e -n '
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
    CTXATTR [i/i/u]
    ACTION
      D EXPRESSION attr=[s/s/c]
        OP2 = (type=<-1> attr=[s/s/c] flags=0)
          IDENT x (type=<-1> attr=[s/s/c] flags=0)
          INT 0x4d2 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
      D PRODUCER attr=[s/s/c]
        FUNC printf (type=<-1> attr=[s/s/c] flags=0)
          STRING "%d\n" (type=<string> attr=[s/s/c] flags=COOK,REF,DPTR)
        ,
          IDENT x (type=<-1> attr=[s/s/c] flags=0)
      D EXPRESSION attr=[s/s/c]
        OP2 = (type=<-1> attr=[s/s/c] flags=0)
          IDENT z (type=<-1> attr=[s/s/c] flags=0)
          OP2 * (type=<-1> attr=[s/s/c] flags=0)
            INT 0x20 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
            OP2 + (type=<-1> attr=[s/s/c] flags=0)
              IDENT x (type=<-1> attr=[s/s/c] flags=0)
              INT 0x12 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
      D EXPRESSION attr=[s/s/c]
        OP2 = (type=<-1> attr=[s/s/c] flags=0)
          OP2 [ (type=<-1> attr=[s/s/c] flags=0)
            IDENT @agg (type=<-1> attr=[s/s/c] flags=0)
            IDENT x (type=<-1> attr=[s/s/c] flags=0)
          FUNC llquantize (type=<-1> attr=[s/s/c] flags=0)
            IDENT x (type=<-1> attr=[s/s/c] flags=0)
          ,
            INT 0x2 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
          ,
            INT 0x3 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
          ,
            INT 0x5 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
          ,
            INT 0x2 (type=<int> attr=[s/s/c] flags=SIGN,COOK)
      D PRODUCER attr=[s/s/c]
        FUNC exit (type=<-1> attr=[s/s/c] flags=0)
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
