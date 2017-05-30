/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2017 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Verify that a dtrace_addr2str() of the null pointer does not
 * resolve to a symbol.
 */

/* @@xfail: bug 26025761 is unfixed */

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
    int n = dtrace_addr2str(handle, (uint64_t) 0, s, sizeof(s));
    printf(" %d chars |%s|\n", n, s);
    dtrace_close(handle);
    return (strchr(s, '`') != NULL); /* ` is an error */
}
