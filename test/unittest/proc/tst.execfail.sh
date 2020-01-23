#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This script tests that -- if a exec(2) fails -- the proc:::exec probe fires,
# followed by the proc:::exec-success probe (in a successful exec(2)).  To
# circumvent any potential shell cleverness, this script generates exec
# failure by generating a file with a bogus interpreter.  (It seems unlikely
# that a shell -- regardless of how clever it claims to be -- would bother to
# validate the interpreter before exec'ing.)
#
# If this fails, the script will run indefinitely; it relies on the harness
# to time it out.
#
# @@xfail: dtv2

script()
{
	$dtrace $dt_flags -s /dev/stdin <<EOF
	proc:::exec
	/ppid == $child && args[0] == "$badexec"/
	{
		self->exec = 1;
	}

	proc:::exec-failure
	/self->exec/
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

badexec=$tmpdir/execfail.ksh.$$
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
