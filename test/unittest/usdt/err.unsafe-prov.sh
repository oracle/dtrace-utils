#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

# In-tree testing vs use-installed testing.
if [ -n "$DTRACE_OPT_DOFSTASHPATH" ]; then
	stash=$DTRACE_OPT_DOFSTASHPATH/stash
	probes=$DTRACE_OPT_DOFSTASHPATH/probes
	rootdir=$tmpdir
else
	stash=/run
	probes=/run
	rootdir=/run
fi

DIRNAME="$tmpdir/usdt-unsafe-prov.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat << EOT > unsafe_prov.c
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <dtrace/ioctl.h>
#include <sys/usdt.h>

/* Provider name with path traversal sequences in it. */
#ifdef DTRACE_OPT_DOFSTASHPATH
#  define PRV "../../../../unsafe"
#else
#  define PRV "../../../../../../../run/unsafe"
#endif
#define PRB "evil"

/*
 * DTrace cannot generate a 'prov' note with unsafe provider name.  So we need
 * to hand-create one.
 */
__asm__ (
	".pushsection .note.usdt,\"?\",@note\n"
	".balign 4\n"
	".4byte 2f-1f\n"
	".4byte 4f-3f\n"
	".4byte 1\n"
	"1: .asciz \"prov\"\n"
	"2: .balign 4\n"
	"3: .asciz \"" PRV "\"\n"
	".balign 4\n"
	".4byte 0\n"			/* attrobutes */
	".4byte 0\n"
	".4byte 0\n"
	".4byte 0\n"
	".4byte 0\n"
	".4byte 1\n"			/* 1 probe */
	".balign 4\n"
	".asciz \"" PRB "\"\n"
	".byte 0\n"			/* No arguments */
	".byte 0\n"
	"4: .balign 4\n"
	".popsection\n"
);

int main(void) {
	char *devname = getenv("DTRACE_DOF_INIT_DEVNAME");
	int fd;

	__asm__ (
		".pushsection .note.usdt,\"?\",@note\n"
		".balign 4\n"
		".4byte 6f-5f\n"
		".4byte 8f-7f\n"
		".4byte 1\n"
		"5: .asciz \"usdt\"\n"
		"6: .balign 4\n"
		"7: .8byte 0\n"
#ifdef __aarch64__
		".8byte %[fn]\n"
#else
		".8byte %p[fn]\n"
#endif
		".asciz \"" PRV "\"\n"
		".asciz \"" PRB "\"\n"
		".byte 0\n"		/* No arguments */
		".asciz \"\"\n"
		"8: .balign 4\n"
		".popsection\n"
		:: [fn] _dt_s(_DT_FN_CONSTRAINT) (__func__)
	);

	if (devname == NULL)
		devname = "/dev/dtrace/helper";

	fd = open(devname, O_RDWR);
	if (fd < 0) {
		perror("open /dev/dtrace/helper");
		return 1;
	}

	if (ioctl(fd, DTRACEHIOC_HASUSDT, &main) < 0) {
		perror("ioctl DTRACEHIOC_HASUSDT");
		return 1;
	}

	printf("%d\n", getpid());

	return 0;
}
EOT

$CC $CFLAGS -o unsafe_prov unsafe_prov.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 0				# Failure: compilation
fi

rm -f $stash/unsafe:unsafe_prov:main:evil

pid=`./unsafe_prov`

if [ -f $stash/unsafe:unsafe_prov:main:evil -o -d $rootdir/unsafe$pid ]; then
	find $stash $probes $rootdir/unsafe$pid -ls
	exit 0				# Failure: directory got created
fi

echo "All OK"

exit 1
