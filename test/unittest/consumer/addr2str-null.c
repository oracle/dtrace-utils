/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@skip: needs work */

/*
 * Verify that a dtrace_addr2str() of the null pointer does not
 * resolve to a symbol.
 */


#include <stdio.h>
#include <string.h>
#include <dtrace.h>

int main(int argc, char **argv)
{
    int err;
    dtrace_hdl_t *handle = dtrace_open(DTRACE_VERSION, 0, &err);
    if (handle == NULL) {
        printf("ERROR: dtrace_open %d |%s|\n",
            err, dtrace_errmsg(handle, err));
        return 1;
    }

    char s[128];
    int n = dtrace_addr2str(handle, (uint64_t)0, s, sizeof(s));
    printf(" %d chars |%s|\n", n, s);
    dtrace_close(handle);
    return strchr(s, '`') != NULL; /* ` is an error */
}
