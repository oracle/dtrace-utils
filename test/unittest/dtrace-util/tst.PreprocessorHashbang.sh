#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

##
#
# ASSERTION:
# When dtrace is invoked via #!, and the -C option is provided, the
# first line (containing the #!) is sliced off.
#
# SECTION: dtrace Utility/-C Option
#
##

dtrace=$1

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

# Rather than real dtrace, use another wrapper as the interpreter.

cat > $tmpdir/test-unittest-dtrace-wrapper <<EOF
#!/bin/bash

script=\$1
shift
exec $dtrace $dt_flags -Cqs \$script \$@
EOF
chmod u+x $tmpdir/test-unittest-dtrace-wrapper

cat > $tmpdir/test-unittest-dtrace-util-preprocessor-hashbang.d <<EOF
#!$tmpdir/test-unittest-dtrace-wrapper

#if 0
this should not be seen
#endif

BEGIN
{
	printf("This test should compile\n");
	exit(0);
}
EOF
chmod u+x $tmpdir/test-unittest-dtrace-util-preprocessor-hashbang.d
$tmpdir/test-unittest-dtrace-util-preprocessor-hashbang.d

if [ "$?" -ne 0 ]; then
	echo $0: dtrace failed
	exit 1
fi

exit 0
