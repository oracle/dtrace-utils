#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies various properties of USDT and pid probes sharing
# underlying probes.

dtrace=$1
usdt=$2
mapping=$3

# Set up test directory.

DIRNAME=$tmpdir/pidprobes.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Create test source files.

if [[ -z $mapping ]]; then
    cat > prov.d <<EOF
provider pyramid {
	probe entry(int, char, int, int);
};
EOF
else
    cat > prov.d <<EOF
provider pyramid {
	probe entry(int a, char b, int c, int d) : (int c, int d, int a, char b);
};
EOF
fi

cat > main.c <<'EOF'
#include <stdio.h>
#include "prov.h"

void foo() {
	int n = 0;

	PYRAMID_ENTRY(2, 'a', 16, 128);
	if (PYRAMID_ENTRY_ENABLED())
		n += 2;
	PYRAMID_ENTRY(4, 'b', 32, 256);
	if (PYRAMID_ENTRY_ENABLED())
		n += 8;
	printf("my result: %d\n", n);
}

int
main(int argc, char **argv)
{
	foo();
}
EOF

# Build the test program.

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >&2
	exit 1
fi
cc $test_cppflags -c main.c
if [ $? -ne 0 ]; then
	echo "failed to compile test" >&2
	exit 1
fi
if [[ `uname -m` = "aarch64" ]]; then
	objdump -d main.o > disasm_foo.txt.before
fi
$dtrace $dt_flags -G -64 -s prov.d main.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >&2
	exit 1
fi
cc $test_ldflags -o main main.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >&2
	exit 1
fi

# Check that the program output is 0 when the USDT probe is not enabled.
# That is, the PYRAMID_ENTRY_ENABLED() is-enabled checks should not pass.

./main > main.out
echo "my result: 0" > main.out.expected
if ! diff -q main.out main.out.expected > /dev/null; then
	echo '"my result"' looks wrong when not using DTrace
	echo === got ===
	cat main.out
	echo === expected ===
	cat main.out.expected
	exit 1
fi

# Run dtrace.

cat >> pidprobes.d <<'EOF'
p*d$target::foo:
{
	printf("%d %s:%s:%s:%s %x\n", pid, probeprov, probemod, probefunc, probename, uregs[R_PC]);
}
EOF

if [[ -n $usdt ]]; then
	echo 'pyramid$target::foo: {' >> pidprobes.d

	if [[ -n $mapping ]]; then
		echo 'printf("%d %s:%s:%s:%s %i %i %i %c\n", pid, probeprov, probemod, probefunc, probename, args[0], args[1], args[2], args[3]);' >> pidprobes.d
	else
		echo 'printf("%d %s:%s:%s:%s %i %c %i %i\n", pid, probeprov, probemod, probefunc, probename, args[0], args[1], args[2], args[3]);' >> pidprobes.d
	fi
	echo '}' >> pidprobes.d
fi

$dtrace $dt_flags -q -c ./main -o dtrace.out -s pidprobes.d > main.out2
if [ $? -ne 0 ]; then
	echo "failed to run dtrace" >&2
	cat main.out2
	cat dtrace.out
	exit 1
fi

# Check that the program output is 10 when the USDT probe is enabled.
# That is, the PYRAMID_ENTRY_ENABLED() is-enabled checks should pass.

echo "my result: 10" > main.out2.expected

if ! diff -q main.out2 main.out2.expected > /dev/null; then
	echo '"my result"' looks wrong when using DTrace
	echo === got ===
	cat main.out2
	echo === expected ===
	cat main.out2.expected
	exit 1
fi

# Get the reported pid.

if [ `awk 'NF != 0 { print $1 }' dtrace.out | uniq | wc -l` -ne 1 ]; then
	echo no unique pid
	cat dtrace.out
	exit 1
fi
pid=`awk 'NF != 0 { print $1 }' dtrace.out | uniq`

# Disassemble foo().

objdump -d main | awk '
BEGIN { use = 0 }             # start by not printing lines
use == 1 && NF == 0 { exit }  # if printing lines but hit a blank, then exit
use == 1 { print }            # print lines
/<foo>:/ { use = 1 }          # turn on printing when we hit "<foo>:" (without printing this line itself)
' > disasm_foo.txt

# From the disassembly, get the PCs for foo()'s instructions.

pcs=`awk '{print strtonum("0x"$1)}' disasm_foo.txt`
pc0=`echo $pcs | awk '{print $1}'`

# From the disassembly, get the PCs for USDT probes.
# Check libdtrace/dt_link.c's arch-dependent dt_modtext() to see
# what sequence of instructions signal a USDT probe.

if [[ `uname -m` = "x86_64" ]]; then

	# It is the first of five nop instructions in a row.
	# So track pc[-6], pc[-5], pc[-4], pc[-3], pc[-2], pc[-1], pc[0]
	# as well as whether they are nop.

	usdt_pcs_all=`awk '
	BEGIN {
		pc6 = -1; is_nop6 = 0;
		pc5 = -1; is_nop5 = 0;
		pc4 = -1; is_nop4 = 0;
		pc3 = -1; is_nop3 = 0;
		pc2 = -1; is_nop2 = 0;
		pc1 = -1; is_nop1 = 0;
	}
	{
		# pc0 is current instruction
		pc0 = strtonum("0x"$1);

		# decide whether it is a nop
		is_nop0 = 0;
		if (NF == 3 &&
		    $2 == "90" &&
		    $3 == "nop")
			is_nop0 = 1;

		# report if pc[-5] is a USDT instruction
		if (is_nop6 == 0 &&
		    is_nop5 == 1 &&
		    is_nop4 == 1 &&
		    is_nop3 == 1 &&
		    is_nop2 == 1 &&
		    is_nop1 == 1 &&
		    is_nop0 == 0)
			print pc5;

		# prepare advance to next instruction
		pc6 = pc5;  is_nop6 = is_nop5;
		pc5 = pc4;  is_nop5 = is_nop4;
		pc4 = pc3;  is_nop4 = is_nop3;
		pc3 = pc2;  is_nop3 = is_nop2;
		pc2 = pc1;  is_nop2 = is_nop1;
		pc1 = pc0;  is_nop1 = is_nop0;
	}' disasm_foo.txt`

	# We expect 4 USDT probes (2 USDT and 2 is-enabled).
	if [ `echo $usdt_pcs_all | awk '{print NF}'` -ne 4 ]; then
		echo ERROR: expected 4 USDT probes but got $usdt_pcs_all
		cat disasm_foo.txt
		exit 1
	fi

	# Separate them into regular and is-enabled PCs.
	# We assume they alternate.
	usdt_pcs=`echo $usdt_pcs_all | awk '{ print $1, $3 }'`
	usdt_pcs_isenabled=`echo $usdt_pcs_all | awk '{ print $2, $4 }'`

elif [[ `uname -m` = "aarch64" ]]; then

	# The initial compilation of foo() makes it obvious where the
	# USDT probes are.  We just have to add the function offset in.
	usdt_pcs=`awk '/<__dtrace_pyramid___entry>/ { print strtonum("0x"$1) + '$pc0' }' disasm_foo.txt.before`
	usdt_pcs_isenabled=`awk '/<__dtraceenabled_pyramid___entry>/ { print strtonum("0x"$1) + '$pc0' }' disasm_foo.txt.before`

	# We expect 4 USDT probes (2 USDT and 2 is-enabled).
	if [ `echo $usdt_pcs | awk '{print NF}'` -ne 2 -o \
	     `echo $usdt_pcs_isenabled | awk '{print NF}'` -ne 2 ]; then
		echo ERROR: expected 4 USDT probes but got $usdt_pcs and $usdt_pcs_isenabled
		cat disasm_foo.txt.before
		exit 1
	fi

else
	echo ERROR unrecognized machine hardware name
	exit 1
fi

# We expect all of the USDT probe PCs to be among the PCs in objdump output.

for pc in $usdt_pcs $usdt_pcs_isenabled; do
	if echo $pcs | grep -q -vw $pc ; then
		echo ERROR: cannot find USDT PC $pc in $pcs
		exit 1
	fi
done

# Get the PC for the pid return probe.  (Just keep it in hex.)

pc_return=`awk '/'$pid' pid'$pid':main:foo:return/ { print $NF }' dtrace.out`

objdump -d main | awk '
/^[0-9a-f]* <.*>:$/ { myfunc = $NF }         # enter a new function
/^ *'$pc_return'/ { print myfunc; exit(0) }  # report the function $pc_return is in
' > return_func.out

echo "<main>:" > return_func.out.expected    # since we use uretprobe for pid return probes, the PC will be in the caller

if ! diff -q return_func.out return_func.out.expected > /dev/null; then
	echo ERROR: return PC looks to be in the wrong function
	echo === got ===
	cat return_func.out
	echo === expected ===
	cat return_func.out.expected
	exit 1
fi

# Build up a list of expected dtrace output:
# - a blank line
# - pid entry
# - pid return
# - pid offset
# - two USDT probes (ignore is-enabled probes)

echo > dtrace.out.expected
printf "$pid pid$pid:main:foo:entry %x\n" $pc0 >> dtrace.out.expected
echo   "$pid pid$pid:main:foo:return $pc_return" >> dtrace.out.expected
for pc in $pcs; do
	printf "$pid pid$pid:main:foo:%x %x\n" $(($pc - $pc0)) $pc >> dtrace.out.expected
done
echo $usdt_pcs | awk '{printf("'$pid' pyramid'$pid':main:foo:entry %x\n", $1);}' >> dtrace.out.expected
echo $usdt_pcs | awk '{printf("'$pid' pyramid'$pid':main:foo:entry %x\n", $2);}' >> dtrace.out.expected
if [[ -n $usdt ]]; then
	if [[ -z $mapping ]]; then
		echo "$pid pyramid$pid:main:foo:entry 2 a 16 128" >> dtrace.out.expected
		echo "$pid pyramid$pid:main:foo:entry 4 b 32 256" >> dtrace.out.expected
	else
		echo "$pid pyramid$pid:main:foo:entry 16 128 2 a" >> dtrace.out.expected
		echo "$pid pyramid$pid:main:foo:entry 32 256 4 b" >> dtrace.out.expected
	fi
fi

# Sort and check.

sort dtrace.out          > dtrace.out.sorted
sort dtrace.out.expected > dtrace.out.expected.sorted

if ! diff -q dtrace.out.sorted dtrace.out.expected.sorted ; then
	echo ERROR: dtrace output looks wrong
	echo === got ===
	cat dtrace.out.sorted
	echo === expected ===
	cat dtrace.out.expected.sorted
	echo === diff ===
	diff dtrace.out.sorted dtrace.out.expected.sorted
	exit 1
fi

echo success
exit 0
