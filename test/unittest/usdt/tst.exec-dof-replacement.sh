#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Make sure that when a program with DOF exec()s another program with
# different DOF, the first program's DOF does not survive.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-exec-dof-replacement.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov1.d <<EOF
provider test_prov {
	probe failed(int);
};
EOF

cat > prov2.d <<EOF
provider test_prov {
	probe succeeded();
};
EOF

if ! { $dtrace $dt_flags -h -s prov1.d && dtrace -h -s prov2.d; } then
	echo "failed to generate header files" >&2
	exit 1
fi

cat > test1.c <<EOF
#include <errno.h>
#include <unistd.h>
#include "prov1.h"

int
main(int argc, char **argv)
{
	execl("test2", "test2", NULL);
	TEST_PROV_FAILED(errno);
	return 1;
}
EOF

cat > test2.c <<EOF
#include <unistd.h>
#include "prov2.h"

int
main(int argc, char **argv)
{
	while(1) {
		sleep(1);
		TEST_PROV_SUCCEEDED();
	}
}
EOF

if ! { ${CC} ${CFLAGS} -c test1.c && ${CC} ${CFLAGS} -c test2.c; } then
	echo "failed to compile test programs" >&2
	exit 1
fi
if ! { $dtrace $dt_flags -G -s prov1.d test1.o && $dtrace $dt_flags -G -s prov2.d test2.o; } then
	echo "failed to create DOF" >& 2
	exit 1
fi
if ! { ${CC} ${LDFLAGS} -o test1 test1.o prov1.o && ${CC} ${LDFLAGS} -o test2 test2.o prov2.o; } then
	echo "failed to link final executables" >& 2
	exit 1
fi

./test1 &
PROC=$!
disown %+

# Wait for the exec, then list all the target's probes.
# We cannot use pure dtrace to do this because it doesn't check
# for new probes enough to hook up new ones, even with -Z and
# even if a target exec()s.
while [[ -d /proc/$PROC ]] && [[ "$(readlink /proc/$PROC/exe)" =~ test1$ ]]; do
    sleep 1
done
$dtrace $dt_flags -p $PROC '-Ptest_prov$target' -l
status2=$?
kill $PROC

if [[ $status1 -ne 0 ]] || [[ $status2 -ne 0 ]]; then
	exit 1
fi
exit 0
