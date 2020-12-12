#!/bin/sh
#
# Oracle Linux DTrace.
# Copyright (c) 2003, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
echo "\
/*\n\
 * Oracle Linux DTrace.\n\
 * Copyright (c) 2003, 2020, Oracle and/or its affiliates. All rights reserved.\n\
 * Use is subject to license terms.\n\
 */\n\
\n\
#include <dt_errtags.h>
\n\
static const char *const _dt_errtags[] = {"

pattern='^	\(D_[A-Z0-9_]*\),*'
replace='	"\1",'

sed -n "s/$pattern/$replace/p" || exit 1

echo "\
};\n\
\n\
static const int _dt_ntag = sizeof(_dt_errtags) / sizeof(_dt_errtags[0]);\n\
\n\
const char *
dt_errtag(dt_errtag_t tag)
{
	return _dt_errtags[(tag > 0 && tag < _dt_ntag) ? tag : 0];
}"

exit 0
