#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xctypes=$tmpdir/ctypes.ctf -n 'BEGIN { exit(0); }'
if [ ! -r $tmpdir/ctypes.ctf ]; then
	echo "ERROR: No ctypes.ctf generated"
	exit 1
fi

if objdump --help | grep ctf >/dev/null; then
	objcopy --add-section=.ctf=$tmpdir/ctypes.ctf /bin/true $tmpdir/ctypes.o
	if [ $? -ne 0 ]; then
		echo "ERROR: Failed to create ELF object from ctypes.ctf"
		exit 1
	fi

	objdump --ctf=.ctf $tmpdir/ctypes.o | \
		gawk '/CTF_VERSION/ { found = 1; next; }
		     found && $1 ~ /0x[0-9A-Fa-f]+:/ { cnt++; next; }
		     END { print "C CTF data" (found ? " " : " NOT ") "found";
			   exit(cnt > 0 ? 0 : 1); }'
	rc=$?
else
	ctf_dump $tmpdir/ctypes.ctf | \
		gawk '/CTF file:/ { found = 1; next; }
		     found && /ID [0-9A-Fa-f]+:/ { cnt++; next; }
		     END { print "C CTF data" (found ? " " : " NOT ") "found";
			   exit(cnt > 0 ? 0 : 1); }'
	rc=$?
fi

rm -f $tmpdir/ctypes.*

exit $rc
