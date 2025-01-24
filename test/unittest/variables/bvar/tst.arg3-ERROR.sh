#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
# ASSERTION: The ERROR arg3 value reports the correct PC value.
#
# SECTION: Variables/Built-in Variables/arg3
##

dtrace=$1

DIRNAME="$tmpdir/arg3-ERROR.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

function dump_files {
	for x in $*; do
		echo ==== $x
		cat $x
	done
}

# Run DTrace, also collecting disassembly.

$dtrace $dt_flags -o D.out -xdisasm=8 -S -qn '
BEGIN { nerrs = 0; }		/* dt_clause_0 */
BEGIN { y = *((int*)64); }	/* dt_clause_1 */
BEGIN { y = *((int*)64); }	/* dt_clause_2 */
BEGIN { y = *((int*)64); }	/* dt_clause_3 */
BEGIN { y = *((int*)64); }	/* dt_clause_4 */
BEGIN { exit(0); }		/* dt_clause_5 */

ERROR
{
	/* Report the problematic PC and continue execution. */
	printf("Error %d at %x\n", ++nerrs, arg3);
}
' >& disasm.out
if [ $? -ne 0 ]; then
	echo DTrace failure
	dump_files disasm.out
	exit 1
fi

# Parse the disassembly output for start PCs for the dt_clause_n.

gawk '
BEGIN { phase = 0 }

# Look for disassembly of dtrace:::BEGIN.
phase == 0 && /^Disassembly of final program dtrace:::BEGIN:$/ { phase = 1; next }
phase == 0 { next }

# Look for a blank line, denoting the start of the variable table.
phase == 1 && NF == 0 { phase = 2; next }
phase == 1 { next }

# Look for a blank line, denoting the start of the BPF relocations.
phase == 2 && NF == 0 { phase = 3; next }
phase == 2 { next }

# Report PC for each "dt_clause_n" (or a blank line to finish).
phase == 3 && /dt_clause_/ { print $3, $4; next }
phase == 3 && NF == 0 { exit(0) }
phase == 3 { next }
' disasm.out > dt_clause_start_pcs.txt
if [ $? -ne 0 ]; then
	echo ERROR: gawk
	dump_files disasm.out dt_clause_start_pcs.txt
	exit 1
fi

# Confirm that we found expected clauses 0-5.

for n in 0 1 2 3 4 5; do
	echo dt_clause_$n >> dt_clause_start_pcs.txt.check
done
if ! gawk '{print $2}' dt_clause_start_pcs.txt | diff - dt_clause_start_pcs.txt.check; then
	echo ERROR: did not find all expected dt_clause_n
	dump_files disasm.out dt_clause_start_pcs.txt
fi

# Dump the error PCs to a file.
#
# BEGIN has 6 clauses (0-5), but 1-4 have the problematic instructions
# we are looking for: "y = *64".  Each time, we check if the address is
# 0, calling dt_probe_error() if necessary.  Then we try to dereference
# the value, again calling dt_probe_error() if necessary.  The second
# dt_probe_error() is the problematic one.  We look for those calls.
for n in 1 2 3 4; do
	# For dt_clause_$n, find the starting PC.
	pc=`gawk '$2 == "dt_clause_'$n'" { print $1 }' dt_clause_start_pcs.txt`

	# Look for the starting PC and then the second dt_probe_error().
	gawk '
	BEGIN { phase = 0 }

	# Look for disassembly of dtrace:::BEGIN.
	phase == 0 && /^Disassembly of final program dtrace:::BEGIN:$/ { phase = 1; next }
	phase == 0 { next }

	# Look for the start PC of dt_clause_n.
	phase == 1 && ($1 + 0) == '$pc' { phase = 2; next }
	phase == 1 { next }

	# Look for the first dt_probe_error() call.
	phase == 2 && /dt_probe_error/ { phase = 3; next }
	phase == 2 { next }

	# Look for the second dt_probe_error() call, reporting the PC and quitting.
	phase == 3 && /dt_probe_error/ { print $1 + 0; exit(0) }
	phase == 3 { next }
	' disasm.out >> err_pcs.txt
done

# Do a sanity check on DTrace's error output.

gawk '/^dtrace: error in dt_clause_[1-4] for probe ID 1 \(dtrace:::BEGIN): invalid address \(0x40) at BPF pc [0-9]*$/ { print $NF }' \
  disasm.out > err_pcs.txt.chk1
if ! diff -q err_pcs.txt err_pcs.txt.chk1; then
	echo ERROR: problem with DTrace error output
	dump_files disasm.out err_pcs.txt err_pcs.txt.chk1
	exit 1
fi

# Check the D script output... the arg3 values reported by the dtrace:::ERROR probe.

if ! gawk 'NF != 0 { print strtonum("0x"$NF) }' D.out | diff -q - err_pcs.txt; then
	echo ERROR: D script output looks wrong
	dump_files D.out err_pcs.txt
	exit 1
fi

echo success

exit 0
