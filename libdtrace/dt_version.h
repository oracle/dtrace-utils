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

/*
 * Versioning definitions
 *
 * These #defines are used in identifier tables to fill in the version fields
 * associated with each identifier.  The DT_VERS_* macros declare the encoded
 * integer values of all versions used so far.  DT_VERS_LATEST must correspond
 * to the latest version value among all versions exported by the D compiler.
 * DT_VERS_STRING must be an ASCII string that contains DT_VERS_LATEST within
 * it along with any suffixes (e.g. Beta).
 *
 * Refer to the Solaris Dynamic Tracing Guide Versioning chapter for an
 * explanation of these DTrace features and their values.
 *
 * When adding a new version:
 *  - Add a new DT_VERS_* macro
 *  - Add the new DT_VERS_* macro at the end of the DTRACE_VERSIONS macro
 *  - Set DT_VERS_LATEST to the new DT_VERS_*
 *  - Update DT_VERS_STRING to reflect the new version
 *
 * NOTE: Although the DTrace versioning scheme supports the labeling and
 *       introduction of incompatible changes (e.g. dropping an interface in a
 *       major release), the libdtrace code does not currently support this.
 *       All versions are assumed to strictly inherit from one another.  If
 *       we ever need to provide divergent interfaces, this will need work.
 *
 * The version number should be increased for every customer visible release
 * of DTrace.
 *  - The major number should be incremented when a fundamental change has been
 *    made that would affect all consumers, and would reflect sweeping changes
 *    to DTrace or the D language.
 *  - The minor number should be incremented when a change is introduced that
 *    could break scripts that had previously worked; for example, adding a new
 *    built-in variable could break a script which was already using that
 *    identifier.
 *  - The micro number should be changed when introducing functionality changes
 *    or major bug fixes that do not affect backward compatibility -- this is
 *    merely to make capabilities easily determined from the version number.
 *    Minor bugs do not require any modification to the version number.
 */
#define	DT_VERS_1_0	DT_VERSION_NUMBER(1, 0, 0)
#define	DT_VERS_1_1	DT_VERSION_NUMBER(1, 1, 0)
#define	DT_VERS_1_2	DT_VERSION_NUMBER(1, 2, 0)
#define	DT_VERS_1_2_1	DT_VERSION_NUMBER(1, 2, 1)
#define	DT_VERS_1_2_2	DT_VERSION_NUMBER(1, 2, 2)
#define	DT_VERS_1_3	DT_VERSION_NUMBER(1, 3, 0)
#define	DT_VERS_1_4	DT_VERSION_NUMBER(1, 4, 0)
#define	DT_VERS_1_4_1	DT_VERSION_NUMBER(1, 4, 1)
#define	DT_VERS_1_5	DT_VERSION_NUMBER(1, 5, 0)
#define	DT_VERS_1_6	DT_VERSION_NUMBER(1, 6, 0)
#define	DT_VERS_1_6_1	DT_VERSION_NUMBER(1, 6, 1)
#define	DT_VERS_1_6_2	DT_VERSION_NUMBER(1, 6, 2)
#define	DT_VERS_1_6_3	DT_VERSION_NUMBER(1, 6, 3)
#define	DT_VERS_1_6_4	DT_VERSION_NUMBER(1, 6, 4)
#define	DT_VERS_2_0	DT_VERSION_NUMBER(2, 0, 0)
#define	DT_VERS_2_0_1	DT_VERSION_NUMBER(2, 0, 1)

#define DTRACE_VERSIONS	{ \
	DT_VERS_1_0,	/* D API 1.0.0 (PSARC 2001/466) Solaris 10 FCS */ \
	DT_VERS_1_1,	/* D API 1.1.0 Solaris Express 6/05 */ \
	DT_VERS_1_2,	/* D API 1.2.0 Solaris 10 Update 1 */ \
	DT_VERS_1_2_1,	/* D API 1.2.1 Solaris Express 4/06 */ \
	DT_VERS_1_2_2,	/* D API 1.2.2 Solaris Express 6/06 */ \
	DT_VERS_1_3,	/* D API 1.3 Solaris Express 10/06 */ \
	DT_VERS_1_4,	/* D API 1.4 Solaris Express 2/07 */ \
	DT_VERS_1_4_1,	/* D API 1.4.1 Solaris Express 4/07 */ \
	DT_VERS_1_5,	/* D API 1.5 Solaris Express 7/07 */ \
	DT_VERS_1_6,	/* D API 1.6 */ \
	DT_VERS_1_6_1,	/* D API 1.6.1 */ \
	DT_VERS_1_6_2,	/* D API 1.6.2 */ \
	DT_VERS_1_6_3,	/* D API 1.6.3 */ \
	DT_VERS_1_6_4,	/* D API 1.6.4 */ \
	DT_VERS_2_0,	/* D API 2.0 */ \
}

#define	DT_VERS_LATEST	DT_VERS_2_0_1
#define	DT_VERS_STRING	"Oracle D 2.0"

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

extern const dt_version_t _dtrace_versions[];
extern const char *const _dtrace_version;

extern char *dt_version_num2str(dt_version_t, char *, size_t);
extern int dt_version_str2num(const char *, dt_version_t *);
extern int dt_version_defined(dt_version_t);

extern int dt_str2kver(const char *, dt_version_t *);

#ifdef  __cplusplus
}
#endif

#endif /* _DT_VERSION_H */
