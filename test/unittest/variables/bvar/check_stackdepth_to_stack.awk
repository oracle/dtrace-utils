#!/usr/bin/gawk -f
#
# Check output of tst.stackdepth-$provider.d tests of the form
#
# {
#     printf("DEPTH %d\n", stackdepth);
#     printf("TRACE BEGIN\n");
#     stack();
#     printf("TRACE END\n");
#     exit(0);
# }
#
# Confirm that the stackdepth information matches the stack information.

/^DEPTH/ {
	depth = int($2);
}

/^TRACE BEGIN/ {
	getline;
	count = 0;
	while ($0 !~ /^TRACE END/) {
		if (NF)
			count++;
		if (getline != 1) {
			print "EOF or error while processing stack\n";
			exit 0;
		}
	}
}

END {
	if (count < depth)
		printf "Stack depth too large (%d > %d)\n", depth, count;
	else if (count > depth)
		printf "Stack depth too small (%d < %d)\n", depth, count;
	else if (count == 0)
		printf "Stack depth is 0\n";
	else
		printf "Stack depth OK\n";
}
