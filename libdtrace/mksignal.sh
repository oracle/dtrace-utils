#!/bin/sh
#
# Oracle Linux DTrace.
# Copyright (c) 2003, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
echo "\
/*\n\
 * Oracle Linux DTrace.\n\
 * Copyright (c) 2003,, Oracle and/or its affiliates. All rights reserved.\n\
 * Use is subject to license terms.\n\
 */\n\
\n"

pattern='^#define[	 ]*_*\(SIG[A-Z0-9]*\)[	 ]\{1,\}\([A-Z0-9]*\).*$'
replace='inline int \1 = \2;@#pragma D binding "1.0" \1'

sed -n "s/$pattern/$replace/p;/SIGRTMAX/q" | tr '@' '\n'
