#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
#
#	To verify egid of current process
#
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
/\$egid != \$1/
{
	exit(1);
}

BEGIN
/\$egid == \$1/
{
	exit(0);
}
EOF
##########################################################################


#chmod 555 the .d file

chmod 555 $dfilename >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "chmod $dfilename failed" >&2
	exit 1
fi

#Get the groupid of the calling process using ps

groupid=`ps -o pid,gid | grep "$$ " | gawk '{print $2}' 2>/dev/null`
if [ $? -ne 0 ]; then
	echo "unable to get uid of the current process with pid = $$" >&2
	exit 1
fi

#Pass groupid as argument to .d file
$dfilename $groupid >/dev/null 2>&1

if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

#Cleanup leftovers

rm -f $dfilename
exit 0
