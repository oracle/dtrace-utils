#!/bin/sh
#
# Oracle Linux DTrace.
# Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
echo "\
/*\n\
 * Oracle Linux DTrace.\n\
 * Copyright © 2005,, Oracle and/or its affiliates. All rights reserved.\n\
 * Use is subject to license terms.\n\
 */\n\
\n\
#include <dtrace.h>\n\
\n\
/*ARGSUSED*/
const char *\n\
dtrace_subrstr(dtrace_hdl_t *dtp, int subr)\n\
{\n\
	switch (subr) {"

awk '
/^#define[ 	]*DIF_SUBR_/ && $2 != "DIF_SUBR_MAX" {
	printf("\tcase %s: return (\"%s\");\n", $2, tolower(substr($2, 10)));
}'

echo "\
	default: return (\"unknown\");\n\
	}\n\
}"
