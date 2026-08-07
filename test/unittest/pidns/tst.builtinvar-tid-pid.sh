#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION:
#	The pid and tid built-in variables report PID namespace-relative IDs
#	when DTrace runs in a non-initial PID namespace.
#
# @@timeout: 35

. test/unittest/pidns/common.bash

check_dtrace_arg "$@"
dtrace=$1
DIRNAME="$tmpdir/pidns-builtinvar-tid-pid.$$.$RANDOM"

cleanup()
{
	rm -rf "$DIRNAME"
}
trap cleanup EXIT

mkdir -p "$DIRNAME" || exit 1
cd "$DIRNAME" || exit 1

cat <<'EOF' > main.c
#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Provide an implementation in case glibc is too old. */
pid_t
gettid(void)
{
	return syscall(__NR_gettid);
}

static void *
foo(void *arg)
{
	int i = 0;

	printf("\n");
	printf("pid     probe expect pid %d tid %d\n", getpid(), gettid());
	printf("profile probe expect pid %d tid %d\n", getpid(), gettid());
	fflush(stdout);

	/* Wait endlessly.  DTrace will kill me when it is done. */
	while (i < 2)
		i ^= 1;

	return 0;
}

int
main(int argc, char **argv)
{
	pthread_t thread;

	pthread_create(&thread, NULL, foo, NULL);
	pthread_join(thread, NULL);

	return 0;
}
EOF

$CC $test_cppflags main.c -lpthread || {
	echo "compilation failed"
	exit 1
}

run_in_pidns bash -s "$dtrace" "$DIRNAME" <<'EOS'
dtrace=$1
dir=$2

cd "$dir" || exit 1
rm -f C.out D.out D.out.sorted

"$dtrace" $dt_flags -o D.out -c ./a.out -qn '
pid$target:a.out:foo:entry
{
	self->mypid = pid;
	printf("pid     probe expect pid %d tid %d\n", pid, tid);
}

profile:::profile-1s
/self->mypid != 0/
{
	printf("profile probe expect pid %d tid %d\n", pid, tid);
	exit(0);
}
' |& sort > C.out
status=$?

if [ $status -ne 0 ]; then
	echo "DTrace failed"
	echo "==== C.out"
	cat C.out
	echo "==== D.out"
	cat D.out
	exit 1
fi

sort D.out > D.out.sorted
if ! diff -q C.out D.out.sorted; then
	echo "ERROR: mismatch"
	echo "==== C.out"
	cat C.out
	echo "==== D.out"
	cat D.out.sorted
	echo "==== diff"
	diff C.out D.out.sorted
	exit 1
fi

exit 0
EOS

exit $?
