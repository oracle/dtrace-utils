/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DT_VERSION_H
#define _DT_VERSION_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <dt_ident.h>
#include <dt_git_version.h>

/*
 * Stability definitions
 *
 * These #defines are used in the tables of identifiers below to fill in the
 * attribute fields associated with each identifier.  The DT_ATTR_* macros are
 * a convenience to permit more concise declarations of common attributes such
 * as Stable/Stable/Common.
 *
 * Refer to the Solaris Dynamic Tracing Guide Stability chapter respectively
 * for an explanation of these DTrace features and their values.
 */
#define DT_ATTR_STABCMN { DTRACE_STABILITY_STABLE, \
	DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON }

#define DT_ATTR_EVOLCMN { DTRACE_STABILITY_EVOLVING, \
	DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON \
}

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
 * You must update DT_VERS_LATEST and DT_VERS_STRING when adding a new version,
 * and then add the new version to the _dtrace_versions[] array declared in
 * dt_open.c..
 *
 * NOTE: Although the DTrace versioning scheme supports the labeling and
 *       introduction of incompatible changes (e.g. dropping an interface in a
 *       major release), the libdtrace code does not currently support this.
 *       All versions are assumed to strictly inherit from one another.  If
 *       we ever need to provide divergent interfaces, this will need work.
 *
 * The version number should be increased for every customer visible release
 * of Solaris. The major number should be incremented when a fundamental
 * change has been made that would affect all consumers, and would reflect
 * sweeping changes to DTrace or the D language. The minor number should be
 * incremented when a change is introduced that could break scripts that had
 * previously worked; for example, adding a new built-in variable could break
 * a script which was already using that identifier. The micro number should
 * be changed when introducing functionality changes or major bug fixes that
 * do not affect backward compatibility -- this is merely to make capabilities
 * easily determined from the version number. Minor bugs do not require any
 * modification to the version number.
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

#define	DT_VERS_LATEST	DT_VERS_2_0
#define	DT_VERS_STRING	"Oracle D 2.0"

#ifdef  __cplusplus
}
#endif

#endif /* _DT_VERSION_H */
