/*
 * Oracle Linux DTrace.
 * Copyright © 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
