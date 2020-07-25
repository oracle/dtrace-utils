#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
#	To verify ppid of child process.
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
/\$ppid != \$1/
{
	exit(1);
}

BEGIN
/\$ppid == \$1/
{
	exit(0);
}
EOF
##########################################################################


#chmod the .d file to 555

chmod 555 $dfilename >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "chmod 555 $dfilename failed" >&2
	exit 1
fi

#Pass current pid (I mean parent pid for .d script).

$dfilename $$ >/dev/null 2>&1

if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

rm -f $dfilename
exit 0
