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
# Copyright 2006 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

#ident	"%Z%%M%	%I%	%E% SMI"

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
dfilename=/var/tmp/$bname.$$.d

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
    2>/var/tmp/err.$$.txt

if [ $? -ne 1 ]; then
	echo "Error in executing $dfilename" >&2
	exit 1
fi

grep "D_MACRO_UNUSED" /var/tmp/err.$$.txt >/dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "Expected error D_MACRO_UNUSED not returned" >&2
	rm -f /var/tmp/err.$$.txt
	exit 1
fi

rm -f $dfilename
rm -f /var/tmp/err.$$.txt

exit 0
