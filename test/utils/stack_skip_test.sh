#!/bin/sh

# Older kernels had a bug in which the leaf frame was replicated.
#
# This bug was fixed on x86_64.  See
#   https://github.com/torvalds/linux/commit/6d08340d1e354787d6c65a8c3cdd4d41ffb8a5ed
#   Revert "perf/x86: Always store regs->ip in perf_callchain_kernel()"
#
# A broader fix, for non-x86, is also expected.  See the thread that starts at:
#   https://lore.kernel.org/all/a38fed68-67bc-98ce-8e12-743342121ae3@oracle.com/
#
# Use this as a .x file to mark tests as "XFAIL" for older kernels with the
# superfluous frame (and therefore exposing a BPF JIT frame, appearing as an
# untranslated 0xffff address).  On older kernels, it is also possible for fbt
# to be implemented with kprobes rather than fprobes;  in this case, the .x
# issues no concerns and the tests are expected to PASS.

$dtrace -c test/triggers/periodic_output -qn '
fbt::hrtimer_nanosleep:entry
{
        stack(1);
        exit(0);
}

ERROR
{
        exit(1);
}' | grep -q 0xffff
if [ $? -eq 0 ]; then
	echo stack appears to have superfluous frame
	exit 1
fi
exit 0
