#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Analyze the loop in the specified workload program ($1), in test/utils/.

prog=`dirname $0`/$1
if [ ! -e $prog ]; then
	echo -1 -1 -1
	exit 1
fi

objdump -d $prog | gawk -v myarch=$(uname -m) '
# decide whether to track instructions (which we number n = 1, 2, 3, ...) or not (n < 0)
# specifically, do not track instructions until we find the disassembly for <main>
BEGIN { n = -1; }
/[0-9a-f]* <main>:$/ { n = 0; next }

# skip over instructions we are not tracking and null instructions
n < 0 { next }
/^ *[0-9a-f]*:	[0 ]*$/ { next }

# if we are tracking but hit a blank line, the <main> disassembly is over and we found no loop
NF == 0 {
	print -1, -1, -1;
	exit 1;
}

# track this instruction
{ n++; instr[n] = $1; }

# remove the instruction hexcodes (between the first two tabs)
{ sub("	[^	]*	", "	"); }

# look for a conditional jump
#   * x86: look for a j* instruction, but not ja or jmp
#   * ARM: look for a b.* instruction
( myarch == "x86_64" && ( /	j/ && !/	ja / && !/	jmp / ) ) ||
( myarch == "aarch64" && /	b\./ ) {

	# look for the jump target among past instructions
	target = $3;
	for (i = 1; i <= n; i++) {
		if (index(instr[i], target)) {
			break;
		}
	}

	# if we found the target, we have identified a loop
	if (i <= n) {
		# report: num of instructions per loop, first PC, and last PC
		print n + 1 - i, instr[i], instr[n];

		# report: all the PCs in the loop (one per line)
		for (i = i; i <= n; i++) print instr[i];
		exit 0;
	}
}' | tr ":" " "      # and get rid of those extraneous ":"

exit $?
