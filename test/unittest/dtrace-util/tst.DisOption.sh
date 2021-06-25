#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# ASSERTION: That the dtrace -xdisasm option can control the listings.

dtrace=$1

DIRNAME="$tmpdir/DisOption.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# create sample D scripts and expected D output

cat << EOF > D1.d
int x;
BEGIN {
    x = 1234;
    y = x;
}
EOF
cat << EOF > D2.d
BEGIN {
    trace(y);
    exit(0);
}
EOF
cat << EOF > D.out
1234
EOF

# run dtrace for various xdisasm settings

function run_dtrace() {
    $dtrace $dt_flags $2 -Sq -s D1.d -s D2.d > $1.out 2> $1.tmp
    if [ $? -ne 0 ]; then
        echo DTrace failed $2
        cat $1.out $1.err
        exit 1
    fi
    # Avoid differenced due to different BOOTTM values.
    awk '/: 18 [0-9] 0 / && /lddw/ {
	    sub(/0x[0-9a-f]+/, 0x0);
	    sub(/[0-9a-f]{8}/, "00000000");
	    print;
	    getline;
	    sub(/0x[0-9a-f]+/, 0x0);
	    sub(/[0-9a-f]{8}/, "00000000");
	    print;
	    next;
	}
	/^R_BPF_INSN_64/ && $4 == "BOOTTM" {
	    $3 = "0";
	    print;
	    next;
	}
	{ print; }' $1.tmp > $1.err
}

run_dtrace def               # default
run_dtrace   0 -xdisasm=1    # listing mode 0
run_dtrace   1 -xdisasm=2    # listing mode 1
run_dtrace   2 -xdisasm=4    # listing mode 2
run_dtrace   3 -xdisasm=8    # listing mode 3
run_dtrace all -xdisasm=15   # all

# check individual D stdout and stderr files

for x in def 0 1 2 3 all; do
    if [ `cat $x.err | wc -l` -lt 10 ]; then
        echo $x.err is suspiciously small
        cat $x.err
        exit 1
    fi
    if ! diff -q $x.out D.out > /dev/null; then
        echo $x.out is not as expected
        cat $x.out
        exit 1
    fi
done

# check default setting

if ! diff -q 0.err def.err > /dev/null; then
    echo it appears that -xdisasm=1 is not the default
    cat 1.err def.err
    exit 1
fi

# split all.err into *.chk files

for x in 0 1 2 3; do
    rm -f $x.chk
    touch $x.chk
done

awk '
BEGIN { f = "/dev/null"; lastlineblank = 0; }
lastlineblank == 1 {
    # if previous line was blank, see if we should change output file
    if (index($0, "Disassembly of clause "        ) == 1) { f = "0.chk" } else
    if (index($0, "Disassembly of program "       ) == 1) { f = "1.chk" } else
    if (index($0, "Disassembly of linked program ") == 1) { f = "2.chk" } else
    if (index($0, "Disassembly of final program " ) == 1) { f = "3.chk" }

    # in any case, print that previous (blank) line
    print "" >> f;
}

# if this line is blank, do not print it yet;  wait to see the next line
NF == 0 {
    lastlineblank = 1;
}
NF != 0 {
    print $0 >> f;
    lastlineblank = 0;
}
' all.err

echo >> 3.chk     # need an extra blank line at the end

# check against *.chk files

for x in 0 1 2 3; do
    if ! diff -q $x.chk $x.err > /dev/null; then
        echo $x.chk does not match $x.err
        diff -u $x.chk $x.err
        echo $x.chk
        cat  $x.chk
        echo $x.err
        cat  $x.err
        exit 1
    fi
done

echo success
exit 0
