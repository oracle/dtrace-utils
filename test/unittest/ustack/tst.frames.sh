#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# @@xfail: dtv2

dtrace=$1
CC=/usr/bin/gcc

DIRNAME="$tmpdir/ustack-frames.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

###########
# make test code
###########

cat > test.c <<EOF
#include <stdio.h>
#include <unistd.h>

/* trigger a syscall in the leaf function */
int fooz() {
	printf("hello world\n");
	fflush(stdout);
	return 1;
}

/* have a relatively deep stack; prevent tail calls */
int fooy() { return fooz() ^ 1; }
int foox() { return fooy() ^ 1; }
int foow() { return foox() ^ 1; }
int foov() { return foow() ^ 1; }
int foou() { return foov() ^ 1; }
int foot() { return foou() ^ 1; }
int foos() { return foot() ^ 1; }
int foor() { return foos() ^ 1; }
int fooq() { return foor() ^ 1; }
int foop() { return fooq() ^ 1; }
int fooo() { return foop() ^ 1; }
int foon() { return fooo() ^ 1; }
int foom() { return foon() ^ 1; }
int fool() { return foom() ^ 1; }
int fook() { return fool() ^ 1; }
int fooj() { return fook() ^ 1; }
int fooi() { return fooj() ^ 1; }
int fooh() { return fooi() ^ 1; }
int foog() { return fooh() ^ 1; }
int foof() { return foog() ^ 1; }
int fooe() { return foof() ^ 1; }
int food() { return fooe() ^ 1; }
int fooc() { return food() ^ 1; }
int foob() { return fooc() ^ 1; }
int fooa() { return foob() ^ 1; }
int fooZ() { return fooa() ^ 1; }
int fooY() { return fooZ() ^ 1; }
int fooX() { return fooY() ^ 1; }
int fooW() { return fooX() ^ 1; }
int fooV() { return fooW() ^ 1; }
int fooU() { return fooV() ^ 1; }
int fooT() { return fooU() ^ 1; }
int fooS() { return fooT() ^ 1; }
int fooR() { return fooS() ^ 1; }
int fooQ() { return fooR() ^ 1; }
int fooP() { return fooQ() ^ 1; }
int fooO() { return fooP() ^ 1; }
int fooN() { return fooO() ^ 1; }
int fooM() { return fooN() ^ 1; }
int fooL() { return fooM() ^ 1; }
int fooK() { return fooL() ^ 1; }
int fooJ() { return fooK() ^ 1; }
int fooI() { return fooJ() ^ 1; }
int fooH() { return fooI() ^ 1; }
int fooG() { return fooH() ^ 1; }
int fooF() { return fooG() ^ 1; }
int fooE() { return fooF() ^ 1; }
int fooD() { return fooE() ^ 1; }
int fooC() { return fooD() ^ 1; }
int fooB() { return fooC() ^ 1; }
int fooA() { return fooB() ^ 1; }
int foo9() { return fooA() ^ 1; }
int foo8() { return foo9() ^ 1; }
int foo7() { return foo8() ^ 1; }
int foo6() { return foo7() ^ 1; }
int foo5() { return foo6() ^ 1; }
int foo4() { return foo5() ^ 1; }
int foo3() { return foo4() ^ 1; }
int foo2() { return foo3() ^ 1; }
int foo1() { return foo2() ^ 1; }
int foo0() { return foo1() ^ 1; }

/* sleep so frames can be resolved before process exits */
int main(int c, char **v) {
	int rc = foo0() ^ 1;
	sleep(5);
	return rc;
}
EOF

###########
# list expected stack
###########

cat > filtered-stack-expect.txt <<EOF
              libc.so.6\`fflush
              a.out\`fooz
              a.out\`fooy
              a.out\`foox
              a.out\`foow
              a.out\`foov
              a.out\`foou
              a.out\`foot
              a.out\`foos
              a.out\`foor
              a.out\`fooq
              a.out\`foop
              a.out\`fooo
              a.out\`foon
              a.out\`foom
              a.out\`fool
              a.out\`fook
              a.out\`fooj
              a.out\`fooi
              a.out\`fooh
              a.out\`foog
              a.out\`foof
              a.out\`fooe
              a.out\`food
              a.out\`fooc
              a.out\`foob
              a.out\`fooa
              a.out\`fooZ
              a.out\`fooY
              a.out\`fooX
              a.out\`fooW
              a.out\`fooV
              a.out\`fooU
              a.out\`fooT
              a.out\`fooS
              a.out\`fooR
              a.out\`fooQ
              a.out\`fooP
              a.out\`fooO
              a.out\`fooN
              a.out\`fooM
              a.out\`fooL
              a.out\`fooK
              a.out\`fooJ
              a.out\`fooI
              a.out\`fooH
              a.out\`fooG
              a.out\`fooF
              a.out\`fooE
              a.out\`fooD
              a.out\`fooC
              a.out\`fooB
              a.out\`fooA
              a.out\`foo9
              a.out\`foo8
              a.out\`foo7
              a.out\`foo6
              a.out\`foo5
              a.out\`foo4
              a.out\`foo3
              a.out\`foo2
              a.out\`foo1
              a.out\`foo0
              a.out\`main
              libc.so.6\`__libc_start_main
EOF

###########
# compile
###########

${CC} -O2 -fno-inline test.c
if [ $? -ne 0 ]; then
        echo "failed to compile test.c" >& 2
        exit 1
fi

###########
# run
###########

$dtrace $dt_flags -c ./a.out -qwn '
    syscall::write:entry
    /pid == $target/
    {
        freopen("ustack.txt");
        ustack();
        freopen("ustack95.txt");
        ustack(95);
        freopen("ustack25.txt");
        ustack(25);
        freopen("");
    }'

status=$?
if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
	rm -f $file
	exit $status
fi

###########
# check
###########

if [ ! -f ustack25.txt ]; then
	echo "missing file ustack25.txt"
	exit 1
fi
if [ ! -f ustack95.txt ]; then
	echo "missing file ustack95.txt"
	exit 1
fi
if [ ! -f ustack.txt ]; then
	echo "missing file ustack.txt"
	exit 1
fi
if ! diff ustack95.txt ustack.txt > /dev/null; then
	echo "ustack(95) differs from ustack()"
	echo "    ustack(95)"
	cat ustack95.txt
	echo "    ustack()"
	cat ustack.txt
	echo "    diff"
	diff -u ustack95.txt ustack.txt
	exit 1
fi
if ! head -26 ustack.txt | diff - ustack25.txt > /dev/null; then
	echo "ustack(25) differs from head of ustack()"
	echo "    ustack(25)"
	cat ustack25.txt
	echo "    ustack() (head)"
	head -26 ustack.txt
	echo "    diff"
	head -26 ustack.txt | diff -u - ustack25.txt
	exit 1
fi

# filter the stack (remove offsets and a few frames) and check

awk '
    /libc\.so\.6`fflush\+0x/ ||
    /a\.out`foo[0-9a-zA-Z]\+0x/ ||
    /a\.out`main\+0x/ ||
    /libc\.so\.6`__libc_start_main\+0x/ {
        sub("+0x.*$", "");
        print;
    }
' ustack.txt > filtered-stack-actual.txt

if ! diff filtered-stack-expect.txt filtered-stack-actual.txt > /dev/null; then
	echo "filtered ustack() differs from expected"
	echo "    ustack()"
	cat ustack.txt
	echo "    ustack() (filtered)"
	cat filtered-stack-actual.txt
	echo "    expected"
	cat filtered-stack-expect.txt
	echo "    diff"
	diff -u filtered-stack-expect.txt filtered-stack-actual.txt
	exit 1
fi

exit 0

