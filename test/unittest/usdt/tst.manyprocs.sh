#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Verify that dtprobed can handle lots of processes.  Also check that it is
# cleaning up wreckage from old dead processes.
#
if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
CC=/usr/bin/gcc
CFLAGS="$test_cppflags"
LDFLAGS="$test_ldflags"

DIRNAME="$tmpdir/usdt-manyprocs.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cat > prov.d <<EOF
provider test_prov {
	probe go();
};
EOF

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >& 2
	exit 1
fi

cat > test.c <<'EOF'
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "prov.h"

int
main(int argc, char **argv)
{
	long instance;

	TEST_PROV_GO();

	instance = strtol(argv[1], NULL, 10);

	/*
	 * Kill every other instance of ourself, so the DOF destructors
	 * never run.
	 */
	if (instance % 2)
		kill(getpid(), SIGKILL);
	return 0;
}
EOF

${CC} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi
$dtrace $dt_flags -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi
${CC} ${LDFLAGS} -o test test.o prov.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

for ((i=0; i < 1024; i++)); do
    ./test $i
done

# When doing in-tree testing, the DOF stash directory
# should contain at most five or so DOFs, even though 512
# processes left stale DOF around.  (Allow up to ten in
# case the most recent cleanup is still underway.)
if [[ $test_libdir != "installed" ]] && [[ -n $DTRACE_OPT_DOFSTASHPATH ]]; then
    NUMDOFS="$(find $DTRACE_OPT_DOFSTASHPATH/stash/dof -type f | wc -l)"
    if [[ $NUMDOFS -gt 10 ]]; then
	echo "DOF stash contains too many old DOFs: $NUMDOFS" >&2
	exit 1
    fi
fi

exit 0
