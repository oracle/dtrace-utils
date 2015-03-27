#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2015 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

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
