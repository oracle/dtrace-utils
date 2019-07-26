/*
 * Oracle Linux DTrace.
 * Copyright (c) 2004, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_STRING_H
#define	_DT_STRING_H

#include <stdio.h>
#include <sys/types.h>
#include <config.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern uint32_t str2hval(const char *, uint32_t);
extern size_t stresc2chr(char *);
extern char *strchr2esc(const char *, size_t);
extern const char *strbasename(const char *);
extern const char *strbadidnum(const char *);
extern int strisglob(const char *);
extern char *strhyphenate(char *);
#ifndef HAVE_STRRSTR
extern char *strrstr(const char *, const char *);
#endif

/*
 * To get around issues with strncpy:
 * - strncpy() use is generally discouraged due to:
 *   - its failure to write null terminating char if no room
 *   - padding null bytes (unnecessary in our use)
 * - compile-time complaints from gcc 8
 */
static inline int
strcpy_safe(char *dst, size_t bufsz, const char *src)
{
	return snprintf(dst, bufsz, "%s", src);
}

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_STRING_H */
