#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION: Built-in variables should not displayed with an offset
#
# SECTION: Variables/Scalar Variables
#
##

dtrace=$1

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
	trace(caller);
	trace(curcpu);
	trace(curthread);
	trace(epid);
	trace(errno);
	trace(execname);
	trace(gid);
	trace(id);
	trace(ipl);
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
	/* trace(uregs[0]); */ /* test this separately until uregs[0] works on all kernels */
	trace(ustackdepth);
	trace(vtimestamp);
	trace(walltimestamp);
	exit(0);
}
' 2>&1 | gawk '
BEGIN {
	rc = 1;
}

/^NAME/ && /KND SCP/ {
	printf "%-16s %-6s %-3s %-3s %-4s %s\n",
	       "NAME", "OFFSET", "KND", "SCP","FLAG", "TYPE";
	while (getline == 1 && NF > 0) {
		if ($3 == "scl" || $3 == "arr") {
			$2 = $5 = "";
			gsub(/ +/, " ");
			printf "%-16s %-6s %-3s %-3s %-4s", $1, "", $2, $3, $4;
			$1 = $2 = $3 = $4 = "";
			gsub(/ +/, " ");
		} else {
			$2 = $6 = "";
			gsub(/ +/, " ");
			printf "%-16s %-6s %-3s %-3s %-4s", $1, $2, $3, $4, $5;
			$1 = $2 = $3 = $4 = $5 = "";
			gsub(/ +/, " ");
		}

		print $0;
		rc = 0;
	}
}

END {
	exit(rc);
}
'

exit $?
