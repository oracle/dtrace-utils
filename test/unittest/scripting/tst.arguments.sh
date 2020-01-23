#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@xfail: dtv2

############################################################################
# ASSERTION:
#	Pass 10 arguments and try to print them.
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
{
	printf("%d %d %d %d %d %d %d %d %d %d", \$1, \$2, \$3, \$4, \$5, \$6,
		\$7, \$8, \$9, \$10);
	exit(0);
}
EOF
##########################################################################


#Call dtrace -C -s <.d>

chmod 555 $dfilename


output=`$dfilename 1 2 3 4 5 6 7 8 9 10 2>/dev/null`

if [ $? -ne 0 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

declare -a outarray
outarray=($output)

if [[ ${outarray[0]} != 1 || ${outarray[1]} != 2 || ${outarray[2]} != 3 || \
	${outarray[3]} != 4 || ${outarray[4]} != 5 || ${outarray[5]} != 6 || \
	${outarray[6]} != 7 || ${outarray[7]} != 8 || ${outarray[8]} != 9 || \
	${outarray[9]} != 10 ]]; then
	echo "Error in output by $dfilename" >&2
	exit 1
fi

rm -f $dfilename
exit 0

