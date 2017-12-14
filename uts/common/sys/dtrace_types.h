/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef DTRACE_SYS_TYPES_H
#define DTRACE_SYS_TYPES_H

#include <sys/types.h>
#include <stdint.h>
#include <sys/ctf_types.h>
#include <endian.h>
#include <unistd.h>

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

#define	SHT_SUNW_dof		0x6ffffff4

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

#ifndef ABS
#define	ABS(a)		((a) < 0 ? -(a) : (a))
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define _LITTLE_ENDIAN 1
#elif __BYTE_ORDER == __BIG_ENDIAN
#define _BIG_ENDIAN 1
#else
#warning Unknown endianness
#endif

/*
 * This comes from <linux/dtrace_os.h>.
 */

typedef uint32_t dtrace_id_t;

#endif
