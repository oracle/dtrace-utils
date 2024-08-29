#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xdtypes=$tmpdir/dtypes.ctf -n 'BEGIN { exit(0); }'
if [ ! -r $tmpdir/dtypes.ctf ]; then
	echo "ERROR: No dtypes.ctf generated"
	exit 1
fi

if objdump --help | grep ctf >/dev/null; then
	objcopy --add-section=.ctf=$tmpdir/dtypes.ctf /bin/true $tmpdir/dtypes.o
	if [ $? -ne 0 ]; then
		echo "ERROR: Failed to create ELF object from dtypes.ctf"
		exit 1
	fi

	objdump --ctf=.ctf $tmpdir/dtypes.o | \
		gawk '/CTF_VERSION/ { found = 1; next; }
		     found && $1 ~ /0x[0-9A-Fa-f]+:/ { cnt++; next; }
		     END { print "D CTF data" (found ? " " : " NOT ") "found";
			   exit(cnt > 0 ? 0 : 1); }'
	rc=$?
else
	ctf_dump $tmpdir/dtypes.ctf | \
		gawk '/CTF file:/ { found = 1; next; }
		     found && /ID [0-9A-Fa-f]+:/ { cnt++; next; }
		     END { print "D CTF data" (found ? " " : " NOT ") "found";
			   exit(cnt > 0 ? 0 : 1); }'
	rc=$?
fi

rm -f $tmpdir/dtypes.*

exit $rc
