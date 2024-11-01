#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that a glob in a pid-provider probe name works.
#

dtrace=$1

DIR=$tmpdir/pid-offsets.$$
mkdir $DIR
cd $DIR

nouter=10
ninner=1000

# Some instructions are "hot" (executed $nouter*$ninner times).
# Some instructions are "cold" (executed only $nouter times).
# Function leaffunc() is hot.
# Function coldfunc() is cold.
# Function loopfunc() has instructions that are:
#     hot   (inside the function's loop)
#     cold (outside the function's loop)
#
# Variable "i" is always manipulated after a function call to prevent
# tail-call optimization.

cat > main.c << EOF
int leaffunc(int i) {
	return 2 * i - 1;
}

int loopfunc(int i) {
	int j;
	for (j = 0; j < $ninner; j++)
		i = 2 * leaffunc(i) - 1;
	return i;
}

int coldfunc(int i) {
	return (2 * loopfunc(i) - 1);
}

int
main(int c, char **v)
{
	int i = 1, j;
	for (j = 0; j < $nouter; j++)
		i = 2 * coldfunc(i) - 1;
	return i;
}
EOF

gcc main.c
if [ $? -ne 0 ]; then
	echo ERROR in compiling
	exit 1
fi

#
# Now run DTrace.
# We are testing a glob in the pid$target:a.out:loopfunc: probe name.
#

$dtrace $dt_flags -xstrsize=16 -qn '
pid$target:a.out:coldfunc:entry,
pid$target:a.out:loopfunc:,
pid$target:a.out:leaffunc:entry
{
	@[probefunc, probename] = count();
}' -c ./a.out -o D.out
if [ $? -ne 0 ]; then
	echo DTrace did not run
	cat D.out
	exit 1
fi

#
# Use objdump and awk to extract the instruction PCs for function loopfunc().
# There should be a loop (jump to an earlier instruction), so also report
# the PCs for the jump and target instructions.  This will help identify
# hot and cold instructions in the function.
#

objdump -d a.out | gawk '
BEGIN {
	pc0 = 0;	# First PC of loopfunc()
	pcjump = 0;	# PC of the jump
	pctarget = 0;	# PC of the target
}

# Look for loopfunc()
!/^[0-9a-f]* <loopfunc>:$/ { next; }

# Process instructions in the loopfunc() function
{
	# Get the first PC
	sub("^0*", "", $1);
	pc0 = strtonum("0x"$1);

	# Walk instructions until a blank line indicates end of function
	getline;
	while (NF > 0) {
		# Figure instruction offset and add to set of offsets
		off = strtonum("0x"$1) - pc0;
		offs[off] = 1;
		printf(" %x", off);

		# Check for jump instruction to some <loopfunc+0x$target> target
		if (match($NF, "^<loopfunc\\+0x[0-9a-f]*>$")) {
			# Figure the offset of the target
			sub("^.*<loopfunc\\+", "", $NF);	# Get "0x$target>"
			sub(">.*$", "", $NF);			# Get "0x$target"
			$NF = strtonum($NF);			# Get $target

			# Use it if a back jump (an offset we have already seen)
			if ($NF in offs) {
				pctarget = $NF;
				pcjump = off;
			}
		}

		# Next line
		getline;
	}

	# Report PCs for jump instruction and target
	printf("\nfrom %x to %x\n", pcjump, pctarget);
	exit(0);
}' > pcs.out
if [ $? -ne 0 ]; then
	echo awk processing of objdump output did not run
	cat   D.out
	cat pcs.out
	exit 1
fi

#
# Use files pcs.out and D.out to check results.
#

gawk '
BEGIN {
   # Determine the expected counts for cold and hot instructions
   ncold = '$nouter';
   nhot = ncold * '$ninner';
   nhot2 = nhot + '$nouter';   # An instruction within the loopfunc() loop may be executed an extra time

   # Track counts for a few of the pid probes specially
   n_coldfunc_entry  = -1;
   n_loopfunc_return = -1;
   n_leaffunc_entry  = -1;
}

#
# Process first file:  pcs.out
# - which PCs are in loopfunc()
# - which PCs are hot and which are cold
#

# Read the list of instruction PCs into array "offs"
NR == 1 { if (split($0, offs) < 4) {
              # We do not know exactly how many instructions to expect,
              # but perform a sanity check for some minimum.
              print "ERROR: few PCs found";
              exit(1)
          };
          next
        }

# Read the jump and target PCs and classify other PCs as hot and cold
NR == 2 { pcjump   = strtonum("0x"$2);
          pctarget = strtonum("0x"$4);
          for (i in offs) {
              off = strtonum("0x"offs[i]);
              if (off >= pctarget && off <= pcjump) {  hot[off] = 1 }
              else                                  { cold[off] = 1 }
          }
          next
        }

#
# Process second file:  D.out
# - get DTrace output and check the counts for the various pid probes
#

NF == 0 { next }

NF != 3 { print "ERROR: line does not have 3 fields:", $0; exit(1) }

# coldfunc:entry
$1 == "coldfunc" && $2 != "entry" { print "ERROR: coldfunc probe other than entry"; exit(1) }
$1 == "coldfunc" && n_coldfunc_entry != -1 { print "ERROR: second coldfunc:entry line"; exit(1) }
$1 == "coldfunc" { n_coldfunc_entry = strtonum($3); next }

# leaffunc:entry
$1 == "leaffunc" && $2 != "entry" { print "ERROR: leaffunc probe other than entry"; exit(1) }
$1 == "leaffunc" && n_leaffunc_entry != -1 { print "ERROR: second leaffunc:entry line"; exit(1) }
$1 == "leaffunc" { n_leaffunc_entry = strtonum($3); next }

# The rest should be loopfunc:*
$1 != "loopfunc" { print "ERROR: probe func is not recognized", $1; exit(1) }

# loopfunc:return
$2 == "return" && n_loopfunc_return != -1 { print "ERROR: second loopfunc:return line"; exit(1) }
$2 == "return" { n_loopfunc_return = strtonum($3); next }

# loopfunc:entry, which we handle as loopfunc:0
$2 == "entry" {
                  if (strtonum($3) != '$nouter') {
                      print "ERROR: loopfunc:entry mismatch";
                      exit(1);
                  }
                  delete cold[0];
                  next
              }

# loopfunc:$off
{
    off = strtonum("0x"$2);
    cnt = strtonum($3);

    if (off in hot) {
        if (cnt != nhot && cnt != nhot2) {
            printf("ERROR: loopfunc:$off mismatch hot: %x %d\n", off, cnt);
            exit(1);
        }
        delete hot[off];
    } else if (off in cold) {
        if (cnt != ncold) {
            printf("ERROR: loopfunc:$off mismatch cold: %x %d\n", off, cnt);
            exit(1);
        }
        delete cold[off];
    } else {
        printf("ERROR: found unanticipated PC: %x\n", off);
        exit(1);
    }
}

# Final checks

END {
    # Check coldfunc:entry, loopfunc:return, and leaffunc:entry counts (and that they were seen)
    if (n_coldfunc_entry  != '$nouter') { print "ERROR: coldfunc:entry mismatch"; exit(1) }
    if (n_loopfunc_return != '$nouter') { print "ERROR: loopfunc:return mismatch"; exit(1) }
    if (n_leaffunc_entry  != nhot) { print "ERROR: leaffunc:entry mismatch"; exit(1) }

    # Report any remaining hot or cold PCs not found (if any)
    num_not_found = 0;
    for (off in  hot) { print "ERROR: hot PC not found:", off; num_not_found++ }
    for (off in cold) { print "ERROR: hot PC not found:", off; num_not_found++ }
    if (num_not_found) exit(1);

    # Done
    print "success";
    exit(0);
}
' pcs.out D.out
if [ $? -ne 0 ]; then
	cat   D.out
	cat pcs.out
	echo ERROR: gawk postprocess
	exit 1
fi

exit 0
