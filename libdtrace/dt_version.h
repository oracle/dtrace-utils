/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DT_VERSION_H
#define _DT_VERSION_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <dt_ident.h>
#include "dt_versions_defs.h"

/*
 * Interfaces for parsing and handling DTrace version strings.  Version binding
 * is a feature of the D compiler that is handled completely independently of
 * the DTrace kernel infrastructure, so the definitions are here in libdtrace.
 * Version strings are compiled into an encoded uint32_t which can be compared
 * using C comparison operators.
 */
#define	DT_VERSION_STRMAX	16	/* enough for "255.4095.4095\0" */
#define	DT_VERSION_MAJMAX	0xFF	/* maximum major version number */
#define	DT_VERSION_MINMAX	0xFFF	/* maximum minor version number */
#define	DT_VERSION_MICMAX	0xFFF	/* maximum micro version number */

#define	DT_VERSION_NUMBER(M, m, u) \
	((((M) & 0xFF) << 24) | (((m) & 0xFFF) << 12) | ((u) & 0xFFF))

#define	DT_VERSION_MAJOR(v)	(((v) & 0xFF000000) >> 24)
#define	DT_VERSION_MINOR(v)	(((v) & 0x00FFF000) >> 12)
#define	DT_VERSION_MICRO(v)	((v) & 0x00000FFF)

typedef uint32_t dt_version_t;

extern const dt_version_t	_dtrace_versions[];
extern const char *const	_dtrace_version;

extern char *dt_version_num2str(dt_version_t, char *, size_t);
extern int dt_version_str2num(const char *, dt_version_t *);
extern int dt_version_defined(dt_version_t);

extern int dt_str2kver(const char *, dt_version_t *);

#ifdef  __cplusplus
}
#endif

#endif /* _DT_VERSION_H */
