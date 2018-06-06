#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
#	To verify sid of current process.
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
/\$sid != \$1/
{
	exit(1);
}

BEGIN
/\$sid == \$1/
{
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

chmod 555 $dfilename

sessionid=`ps -o pid,sid | grep "$$ " | awk '{print $2}' 2>/dev/null`
if [ $? -ne 0 ]; then
	echo "unable to get sid of the current process with pid = $$" >&2
	exit 1
fi

$dfilename $sessionid >/dev/null 2>&1

if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

rm -f $dfilename
exit 0
