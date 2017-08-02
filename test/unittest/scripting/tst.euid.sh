#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
#	To verify euid of current process
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
dfilename=/var/tmp/$bname.$$

## Create .d file
##########################################################################
cat > $dfilename <<-EOF
#!$dtrace -qs


BEGIN
/\$euid != \$1/
{
	exit(1);
}

BEGIN
/\$euid == \$1/
{
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

chmod 555 $dfilename

userid=`ps -o pid,uid | grep "$$ " | awk '{print $2}' 2>/dev/null`
if [ $? -ne 0 ]; then
	print -u2 "unable to get uid of the current process with pid = $$"
	exit 1
fi

$dfilename $userid >/dev/null 2>&1

if [ $? -ne 0 ]; then
	print -u2 "Error in executing $dfilename"
	exit 1
fi

rm -f $dfilename
exit 0
