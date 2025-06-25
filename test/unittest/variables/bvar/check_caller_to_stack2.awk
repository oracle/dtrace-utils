#!/usr/bin/gawk -f

# Check output of tst.caller-$provider.d tests of the form
#
# {
#     stack(2);
#     sym(caller);
# }
#
# Confirm that the caller information matches the stack information.

# Look for the first nonblank line.
NF != 0 {
	# It is the current frame.  Skip it.
	if (getline != 1) { print "ERROR: missing expected output"; exit(1); }

	# Now we have the caller frame.  Strip off the offset.
	expect=$1
	sub("+0x[0-9a-f]*$", "", expect);

	# Finally, get the sym(caller) output.
	if (getline != 1) { print "ERROR: missing expected output"; exit(1); }

	# Compare.
	if (expect == $1) {
		print "success";
		exit(0);
	} else {
		print "ERROR: expect", expect, "but got", $1;
		exit(1);
	}
}
