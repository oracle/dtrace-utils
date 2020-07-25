#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
#	To verify uid of current process.
#
# SECTION: Scripting
#
############################################################################

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
bname=`basename $0`
dfilename=$tmpdir/$bname.$$.d

## Create .d file
##########################################################################
cat > $dfilename <<-EOF
#!$dtrace -qs


BEGIN
/\$uid != \$1/
{
	exit(1);
}

BEGIN
/\$uid == \$1/
{
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

chmod 555 $dfilename

userid=`id -u`
if [ $? -ne 0 ]; then
	echo "unable to get uid of the current process" >&2
	exit 1
fi

$dfilename $userid >/dev/null

if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

rm -f $dfilename
exit 0
