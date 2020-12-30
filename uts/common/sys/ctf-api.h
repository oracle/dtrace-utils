/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DT_CTF_API_H
#define _DT_CTF_API_H

#include <config.h>

#ifdef HAVE_LIBCTF
# include <ctf-api.h>
# define ctf_close(fp)	ctf_file_close(fp)
#else
# include <sys/ctf_api.h>
#endif

#endif /* _DT_CTF_API_H */
