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
 * Copyright 2011 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef DTRACE_SYS_TYPES_H
#define DTRACE_SYS_TYPES_H

#include <sys/types.h>
#include <sys/ctf_types.h>
#include <unistd.h>

typedef unsigned long   psaddr_t;
typedef int psetid_t;
typedef enum { B_FALSE, B_TRUE} boolean_t;

/*
 * Strictly conforming ANSI C environments prior to the 1999
 * revision of the C Standard (ISO/IEC 9899:1999) do not have
 * the long long data type.
 */
#if defined(_LONGLONG_TYPE)
typedef long long               longlong_t;
typedef unsigned long long      u_longlong_t;
#else
/* used to reserve space and generate alignment */
typedef union {
        double  _d;
        int32_t _l[2];
} longlong_t;
typedef union {
        double          _d;
        uint32_t        _l[2];
} u_longlong_t;
#endif  /* defined(_LONGLONG_TYPE) */

typedef longlong_t      offset_t;

typedef unsigned long long hrtime_t;

#define PAGESIZE        (_sysconf(_SC_PAGESIZE))

#define	SHT_SUNW_dof		0x6ffffff4

#define	EM_AMD64	62		/* AMDs x86-64 architecture */

#define	STV_ELIMINATE	6

/*
 * This is unnecessary on OEL6, but necessary on the snapshot build host, which
 * runs OEL5.
 */
#if !defined(PN_XNUM)
#define PN_XNUM 0xffff		        /* extended program header index */
#endif


/*
 *      Definitions for commonly used resolutions.
 */
#define SEC             1
#define MILLISEC        1000
#define MICROSEC        1000000
#define NANOSEC         1000000000

#define SIG2STR_MAX     32

/*
 * p_flag codes
 *
 * note that two of these flags, SMSACCT and SSYS, are exported to /proc's
 * psinfo_t.p_flag field.  Historically, all were, but since they are
 * implementation dependant, we only export the ones people have come to
 * rely upon.  Hence, the bit positions of SSYS and SMSACCT should not be
 * altered.
 */
#define	SSYS	   0x00000001	/* system (resident) process */


#ifndef ABS
#define	ABS(a)		((a) < 0 ? -(a) : (a))
#endif


#endif
