#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@timeout: 80

dtrace=$1

DIRNAME=$tmpdir/pid-dash.$$.$RANDOM
mkdir $DIRNAME
cd $DIRNAME

# Make trigger program.

cat << EOF > main.c
int foo0(int i) {
    int j, k;

    j = i ^ 1; k = j ^ 1; i = k ^ 1;
    j = i ^ 1; k = j ^ 1; i = k ^ 1;
    j = i ^ 1; k = j ^ 1; i = k ^ 1;

    return i;
}
int foo1(int i) {
    int j, k;

    j = i ^ 1; k = j ^ 1; i = k ^ 1;
    j = i ^ 1; k = j ^ 1; i = k ^ 1;
    j = i ^ 1; k = j ^ 1; i = k ^ 1;

    return i;
}
int foo2(int i) {
    int j, k;

    j = i ^ 1; k = j ^ 1; i = k ^ 1;
    j = i ^ 1; k = j ^ 1; i = k ^ 1;
    j = i ^ 1; k = j ^ 1; i = k ^ 1;

    return i;
}
int main(int c, char **v) {
    int i = 0;

    i = foo0(i) ^ i;
    i = foo1(i) ^ i;
    i = foo2(i) ^ i;

    return i;
}
EOF

$CC main.c
if [ $? -ne 0 ]; then
	echo ERROR compile
	exit 1
fi

# Loop over functions in the program.

for func in foo0 foo1 foo2 main; do
	# For each function, get the absolute and relative
	# (to the function) address of some instruction in
	# the function.
	read ABS REL <<< `$OBJDUMP -d a.out | awk '
	  # Look for the function.
	  /^[0-9a-f]* <'$func'>:$/ {

	  # Get the first instruction and note the base address of the function.
	  getline; sub(":", ""); base = strtonum("0x"$1);

	  # Get the next instruction.
	  getline;

	  # Get the next instruction.  Note its PC.
	  getline; sub(":", ""); pc = strtonum("0x"$1);

	  # Print the address, both absolute and relative.
	  printf("%x %x\n", pc, pc - base);
	  exit(0);
	  }'`

	# Write the expected output to the compare file.
	echo got $ABS $func:$REL >> dtrace.exp
	echo got $ABS "-":$ABS   >> dtrace.exp

	# Write the actual dtrace output to the output file.
	# Specify the pid probe with both relative and absolute
	# forms.
	for probe in $func:$REL "-:$ABS"; do
		$dtrace -c ./a.out -o dtrace.out -qn '
		    pid$target:a.out:'$probe'
		    { printf("got %x %s:%s", uregs[R_PC], probefunc,
							  probename); }'
		if [ $? -ne 0 ]; then
			echo ERROR: dtrace
			cat dtrace.out
			exit 1
		fi
	done
done

# Check results.

if ! diff -q dtrace.exp dtrace.out; then
	echo ERROR: 
	echo "==== expected"
	cat dtrace.exp
	echo "==== actual"
	cat dtrace.out
	exit 1
fi

echo success
exit 0
