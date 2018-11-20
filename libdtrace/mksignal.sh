#!/bin/sh
#
# Oracle Linux DTrace.
# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

echo '/*'
echo ' * Oracle Linux DTrace.'
echo ' * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.'
echo ' * Use is subject to license terms.'
echo ' *'
echo ' */'

# The input is from /usr/include/signal.h and files it includes.
# Signal numbers are sometimes redefined or defined in terms of
# other signal numbers, some of which have not yet been defined.
# All that is fine for the C preprocessor, but not for signal.d.
# So, we read them all in.  Then, we print them out, looking up
# numerical values where necessary.

# The search on /^#define.../ should be unnecessary,
# since libdtrace/Build does it before calling this script.

awk '
    /SIGEV_/ || /SIG_/ || /SIGRTMIN/ || /SIGRTMAX/ || /SIGSTKSZ/ { next }
    /^#define[[:blank:]]*SIG[[:alnum:]]*[[:blank:]]/ { signum[$2] = $3 }
    END {
        for (sig in signum) {
            num = signum[sig];
            if (match(num, "SIG")) { num = signum[num] };
            printf("inline int %s = %s;\n", sig, num);
            printf("#pragma D binding \"1.0\" %s\n\n", sig);
        }
    }'
