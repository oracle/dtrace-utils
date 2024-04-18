#!/bin/sh
#
# Oracle Linux DTrace.
# Copyright (c) 2012, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

awk 'BEGIN {
    print "\n\
/*\n\
 * Oracle Linux DTrace.\n\
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.\n\
 * Use is subject to license terms.\n\
 */\n\
";
}
{
    if ($3+0 == 0) {
	val=errno[$3];
    } else {
	val=$3;
    }
    errno[$2]=$3;

    print "inline int " $2 " = " val ";";
    print "#pragma D binding \"1.0\" " $2;
}'
