/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/dtrace_types.h>
#include <time.h>
#include <pthread.h>

hrtime_t
gethrtime(void)
{
        struct timespec sp;
        int ret;
        long long v;

        ret = clock_gettime(CLOCK_REALTIME, &sp);
        if (ret)
                return 0;

        v = 1000000000LL;
        v *= sp.tv_sec;
        v += sp.tv_nsec;

        return v;
}
