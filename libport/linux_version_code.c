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
 * Copyright 2016 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/utsname.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <dt_debug.h>

static unsigned long version;

static int
parse_tok(char *utsrelease, char **saveptr)
{
	const char *tok;
        int ver;

        tok = strtok_r(utsrelease, ".-+", saveptr);
        if (!tok)
                return -1;

        errno = 0;
        ver = strtol(tok, NULL, 10);
        if (errno)
                return -1;
        return ver;
}

unsigned long
linux_version_code(void)
{
        struct utsname uts;
        char *saveptr;

        int major;
        int minor;
        int micro;

        if (version)
                return version;

        uname(&uts);

        major = parse_tok(uts.release, &saveptr);
        if (major < 0)
                goto err;
        minor = parse_tok(NULL, &saveptr);
        if (minor < 0)
                goto err;
        micro = parse_tok(NULL, &saveptr);
        if (micro < 0)
                goto err;

        version = (major << 16) + (minor << 8) + micro;

        return version;
err:
        uname(&uts);
        dt_dprintf("Cannot derive kernel version code from utsrelease of %s\n",
            uts.release);
        return 0;
}
