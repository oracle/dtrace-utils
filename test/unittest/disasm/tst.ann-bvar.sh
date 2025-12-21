#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1
tmpfile=$tmpdir/tst.ann_bvar.$$

$dtrace $dt_flags -Sen '
sdt:task::task_rename
{
	trace(arg0);
	trace(arg1);
	trace(arg2);
	trace(arg3);
	trace(arg4);
	trace(arg5);
	trace(arg6);
	trace(arg7);
	trace(arg8);
	trace(arg9);
	trace(args[0]);
	trace(args[1]);
	trace(args[2]);
/*	trace(args[3]); */
	trace(caller);
	trace(curcpu);
	trace(curthread);
	trace(epid);
	trace(errno);
	trace(execname);
	trace(gid);
	trace(id);
/*	trace(ipl); */
	trace(pid);
	trace(ppid);
	trace(probefunc);
	trace(probemod);
	trace(probename);
	trace(probeprov);
	trace(stackdepth);
	trace(tid);
	trace(timestamp);
	trace(ucaller);
	trace(uid);
	trace(ustackdepth);
/*	trace(vtimestamp); */
	trace(walltimestamp);
	exit(0);
}
' >& $tmpfile
if [ $? -ne 0 ]; then
	echo "ERROR: DTrace not successful"
	cat $tmpfile
	exit 1
fi

gawk '/ call dt_bvar_/ { sub(/^[^:]+: /, ""); print; }' $tmpfile

exit $?
