#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/cpu-syscall.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# Make trigger code and compile it.
#

cat << EOF > main.c
#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

int ncpus;
cpu_set_t *mask;
size_t size;

/*
 * Grow the CPU mask as needed.
 *
 * Other ways of determining the number of CPUs available on the system:
 * - inspecting the contents of /proc/cpuinfo
 * - using sysconf(3) to obtain _SC_NPROCESSORS_CONF and _SC_NPROCESSORS_ONLN
 * - inspecting the list of CPU directories under /sys/devices/system/cpu/
 */
int grow_mask() {
	ncpus = 1024;
	while ((mask = CPU_ALLOC(ncpus)) != NULL &&
	       sched_getaffinity(0, CPU_ALLOC_SIZE(ncpus), mask) != 0 &&
	       errno == EINVAL) {
		CPU_FREE(mask);
		errno = 0;
		ncpus *= 2;
	}
	if (mask == NULL || (errno != 0 && errno != EINVAL))
		return 1;
	size = CPU_ALLOC_SIZE(ncpus);
	return 0;
}

int bind_to_cpu(size_t cpu) {
	CPU_ZERO_S(size, mask);
	CPU_SET_S(cpu, size, mask);
	if (sched_setaffinity(0, size, mask) != 0)
		return 1;
	return 0;
}

int main(int c, char **v) {
	int i, fd = open("/dev/null", O_WRONLY);

	if (fd == -1)
		return 1;

	if (grow_mask() == -1)
		return 1;

	/* Loop over CPUs (in argv[]). */
        for (i = 1; i < c; i++) {
		/* Bind to CPU. */
		if (bind_to_cpu(atol(v[i])) == -1)
			return 1;

		/* Call write() from CPU. */
		write(fd, &i, sizeof(i));
	}

	close(fd);
	CPU_FREE(mask);

	return 0;
}
EOF

gcc main.c
if [ $? -ne 0 ]; then
	echo ERROR compilation failed
	exit 1
fi

#
# Get CPU list and form expected-results file.
#

cpulist=`gawk '/^processor[ 	]: [0-9]*$/ { print $3 }' /proc/cpuinfo`
echo $cpulist

echo > expect.txt
for cpu in $cpulist; do
	echo $cpu >> expect.txt
done

#
# Run DTrace (without -xcpu).  Check that all CPUs appear.
#

$dtrace $dt_flags \
    -qn 'syscall::write:entry { printf("%d\n", cpu); }' \
    -c "./a.out $cpulist" | sort -n | uniq > actual.txt
if [ $? -ne 0 ]; then
	echo ERROR dtrace failed
	exit 1
fi

if ! diff -q expect.txt actual.txt > /dev/null; then
	echo ERROR did not see expected CPUs in baseline case
	echo expect $cpulist
	echo actual:
	cat actual.txt
	exit 1
fi

#
# Pick a CPU at random.
#

cpus=( $cpulist )
ncpus=${#cpus[@]}
cpu0=${cpus[$((RANDOM % $ncpus))]}

#
# Run DTrace (with -xcpu).  Check that only the specified CPU appears.
#

$dtrace $dt_flags -xcpu=$cpu0 \
    -qn 'syscall::write:entry { printf("%d\n", cpu); }' \
    -c "./a.out $cpulist" | sort -n | uniq > actual.txt
echo > expect.txt
echo $cpu0 >> expect.txt

if ! diff -q expect.txt actual.txt > /dev/null; then
	echo ERROR did not see expected CPUs in cpu $cpu0 case, saw:
	cat actual.txt
	exit 1
fi

exit 0
