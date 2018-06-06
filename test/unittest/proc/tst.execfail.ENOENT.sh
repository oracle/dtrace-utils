#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script is identical to tst.execfail.ksh -- but it additionally checks
# that errno is set to ENOENT in the case that an interpreter can't be
# found.
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::exec
	/ppid == $child && args[0] == "$badexec"/
	{
		self->exec = 1;
	}

	proc:::exec-failure
	/self->exec && args[0] == -ENOENT/
	{
		exit(0);
	}
EOF
}

sleeper()
{
	while true; do
		sleep 1
		$badexec
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

badexec=$tmpdir/execfail.ENOENT.ksh.$$
dtrace=$1

cat > $badexec <<EOF
#!/this_is_a_bogus_interpreter
EOF

chmod +x $badexec

sleeper &
child=$!
disown %+

script
status=$?

kill $child
rm $badexec

exit $status
