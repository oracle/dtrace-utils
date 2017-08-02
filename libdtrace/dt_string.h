/*
 * Oracle Linux DTrace.
 * Copyright (c) 2004, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_STRING_H
#define	_DT_STRING_H

#include <sys/types.h>
#include <strings.h>
#include <config.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern size_t stresc2chr(char *);
extern char *strchr2esc(const char *, size_t);
extern const char *strbasename(const char *);
extern const char *strbadidnum(const char *);
extern int strisglob(const char *);
extern char *strhyphenate(char *);
#ifndef HAVE_STRRSTR
extern char *strrstr(const char *, const char *);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_STRING_H */
