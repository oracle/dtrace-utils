#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/builtinvar-tid_pid.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Create trigger program.

cat << EOF > main.c
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

/* Provide an implementation in case glibc is too old. */
pid_t gettid(void)
{
        return syscall(__NR_gettid);
}

static void * foo(void *arg) {
	int i = 0;

	/*
	 * Each thread reports the pid and tid values it expects.
	 * (Expect the same values for both the pid and profile probes.)
	 */
	printf("pid     probe expect pid %d tid %d\n", getpid(), gettid());
	printf("profile probe expect pid %d tid %d\n", getpid(), gettid());
	fflush(stdout);

	/* Wait endlessly.  DTrace will kill me when it is done. */
	while (i < 2)
		i ^= 1;

	return 0;
}

int main(int c, char **v) {
	pthread_t mythr;

	/* Create a thread. */
	pthread_create(&mythr, NULL, &foo, NULL);

	/* Also report the pthread_t. */
	printf("created pthread_t %lld\n\n", mythr);
	fflush(stdout);

	/* Wait endlessly.  DTrace will kill me when it is done. */
	pthread_join(mythr, NULL);

	return 0;
}
EOF

# Compile the trigger program.

$CC $test_cppflags main.c -lpthread
if [ $? -ne 0 ]; then
    echo compilation failed
    exit 1
fi

# Run DTrace.

rm -f C.out D.out
$dtrace $dt_flags -o D.out -c ./a.out -qn '
/* Report pid and tid from a pid-provider probe. */
pid$target:a.out:foo:entry
{
	self->mypid = pid;
	printf("pid     probe expect pid %d tid %d\n", pid, tid);
}

/* Report pid and tid from a profile-provider probe.  Look for the thread we created. */
profile:::profile-1s
/self->mypid != 0/
{
	printf("profile probe expect pid %d tid %d\n", pid, tid);
	exit(0);
}

/* While we are at it, check the pthread_t returned via pthread_create(). */
pid$target::pthread_create:entry
{
	self->thrid_p = (uintptr_t) arg0;
}
pid$target::pthread_create:return
{
	printf("created pthread_t %lld\n", *((long long *)copyin(self->thrid_p, sizeof(long long *))));
	self->thrid_p = 0
}' |& sort > C.out
if [ $? -ne 0 ]; then
	echo DTrace failed
	echo ==== C.out
	cat C.out
	echo ==== D.out
	cat D.out
	exit 1
fi

# Compare the C and D output.

sort D.out > D.out.sorted
if ! diff -q C.out D.out.sorted ; then
	echo ERROR: mismatch
	echo ==== C.out
	cat C.out
	echo ==== D.out
	cat D.out.sorted
	echo ==== diff
	diff C.out D.out.sorted
	exit 1
fi

echo success
exit 0
