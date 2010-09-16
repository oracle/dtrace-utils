#ifndef TYPES_OFF_H
#define TYPES_OFF_H

#include <stdint.h>
#include <sys/types.h>

/*
 *  * Strictly conforming ANSI C environments prior to the 1999
 *   * revision of the C Standard (ISO/IEC 9899:1999) do not have
 *    * the long long data type.
 *     */
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

/*
 * The size of off_t and related types depends on the setting of
 * _FILE_OFFSET_BITS.  (Note that other system headers define other types
 * related to those defined here.)
 *
 * If _LARGEFILE64_SOURCE is defined, variants of these types that are
 * explicitly 64 bits wide become available.
*/
#ifndef _OFF_T
#define _OFF_T

#if defined(_LARGEFILE64_SOURCE)
#ifdef _LP64
typedef off_t           off64_t;        /* offsets within files */
#else
typedef longlong_t      off64_t;        /* offsets within files */
#endif
#else
typedef __off64_t	off64_t;
#endif  /* _LARGEFILE64_SOURCE */

#endif /* _OFF_T */

typedef u_longlong_t    core_content_t;

#endif
