#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@reinvoke-failure: 1

dtrace=$1

DIRNAME="$tmpdir/flowindent.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Create the trigger.  Produce the following call stack 3 times:
#         main()
#           -> foo1()
#             -> foo2()
#               -> foo3()
#                 -> foo4()
#                   => syscall::write
#                     -> fbt:vmlinux:__*_sys_write

cat << EOF > main.c
#include <stdio.h>

int foo4(int x) {
	printf("%d", x);
	fflush(stdout);   /* prevent buffering the printf */
	return x + 1;
}

int foo3(int x) { return foo4(x) + 1; }
int foo2(int x) { return foo3(x) + 1; }
int foo1(int x) { return foo2(x) + 1; }

int main(int c, char **v) {
	int iter, value = 0;
	for (iter = 0; iter < 3; iter++) {
		value = foo1(value);
	}
	return 0;
}
EOF

$CC main.c
if [ $? -ne 0 ]; then
	echo ERROR: compilation
	exit 1
fi

# Run with flowindent, equivalently specifying either -F or -xflowindent.
# Ignore the trigger (a.out) output.
# Use a long switchrate to reduce the chances of interruption from dt_consume_cpu(),
# which resets the flowindent algorithm.

for my_opt in "-F" "-xflowindent" ; do
	$dtrace $dt_flags -c "./a.out > /dev/null" $my_opt -xswitchrate=1s -Zqn '
	/*
	 * Probe on each foo* entry and return.
	 * Report the cpu for more debugging in case of failure.
	 */
	pid$target::foo*:entry,
	pid$target::foo*:return { printf("%d foo\n", cpu); }

	/*
	 * Add a second statement for those foo* probes.
	 */
	pid$target::foo*:entry,
	pid$target::foo*:return { printf("%d foo again\n", cpu); }

	/*
	 * And add some syscall and kernel probes.
	 */
	syscall::write:,
	fbt:vmlinux:__arm64_sys_write:,
	fbt:vmlinux:__x64_sys_write:
	/pid == $target/
	{
		printf("%d write\n", cpu);
	}' >> D.out 2>&1

	if [ $? -ne 0 ]; then
		echo ERROR: D script
		exit 1
	fi
done

# Construct the file to compare D output.  Note:
#   - Each entry/return changes indentation only once,
#       even if there are multiple statements per probe.
#   - The syscall entry/return uses => and <=.
# Each iteration happens to have 20 lines of output.
# There are 2 runs (-F and -xflowindent).
# Each run has 3 iterations, but we will discard the first one.
# So the cmp file has 2*(3-1) = 4 iterations in total.

for iter in `seq 4`; do
cat << EOF >> D.cmp
 -> foo1                                  foo
  | foo1:entry                            foo again
   -> foo2                                foo
    | foo2:entry                          foo again
     -> foo3                              foo
      | foo3:entry                        foo again
       -> foo4                            foo
        | foo4:entry                      foo again
         => write                         write
           -> write                       write
           <- write                       write
         <= write                         write
       <- foo4                            foo
      | foo4:return                       foo again
     <- foo3                              foo
    | foo3:return                         foo again
   <- foo2                                foo
  | foo2:return                           foo again
 <- foo1                                  foo
| foo1:return                             foo again
EOF
done

# Postprocess the D output.

awk '
# If there is a break in the output, restart the line number.
NF == 0 || /FUNCTION/ { lineno = 0; next }

# Increment the line number.
{ lineno++; }

# We do not want to be interrupted by dt_consume_cpu() calls.
# Therefore we use a long switchrate to throttle the consumer.
# Since throttling does not start immediately, however, we also
# discard the first iteration of output in each run.  We saw
# earlier there are 20 lines of output per iteration.
lineno <= 20 { next }

# Then print.  The cpu IDs are removed for the sake of comparison.
# The cpu IDs were printed in the first place since it is possible
# that the trigger migrates from one cpu to another, disrupting the
# flowindent algorithm.  That is unlikely for such a short-running
# program.  Still, if we get unlucky, at least there is more data
# for debugging.  If migration proves to become an issue, consider
# using taskset to bind the trigger to a cpu.
{
	sub(/__arm64_sys_write/, "write            ");  # scrub arm64 label
	sub(  /__x64_sys_write/, "write          "  );  # scrub   x64 label
	sub( /[0-9]+ foo/, "foo");                      # remove cpu ID
	sub( /[0-9]+ write/, "write");                  # remove cpu ID
	print;
}' D.out > D.post
if [ $? -ne 0 ]; then
	echo ERROR: awk
	exit 1
fi

# Report.

if diff -q D.cmp D.post ; then
	echo success
	exit 0
fi
cat D.out
echo ====
diff D.cmp D.post
echo ====

exit 1
