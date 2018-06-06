#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

############################################################################
# ASSERTION:
# Attempt to pass some arguments and try not to print it.
#
# SECTION: Scripting
#
############################################################################

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
bname=`/bin/basename $0`
dfilename=$tmpdir/$bname.$$.d
errorfile=$tmpdir/$bname.$$.err

## Create .d file
##########################################################################
cat > $dfilename <<-EOF
#!$dtrace -qs

BEGIN
{
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

$dtrace $dt_flags -x errtags -s $dfilename "this is a test" 1>/dev/null \
    2>$errorfile

if [ $? -ne 1 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

grep "D_MACRO_UNUSED" $errorfile >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "Expected error D_MACRO_UNUSED not returned" >&2
	rm -f $errorfile
	exit 1
fi

rm -f $dfilename
rm -f $errorfile

exit 0
