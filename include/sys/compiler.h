/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2011, 2015 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_COMPILER_H
#define _SYS_COMPILER_H

/*
 * Attributes that vary on a compiler-by-compiler basis.
 */

#if defined (__GNUC__)

/*
 * GCC. We assume that all compilers claiming to be GCC support sufficiently
 * many GCC attributes that the code below works. If some non-GCC compilers
 * masquerading as GCC in fact do not implement these attributes, version checks
 * may be required.
 */

/*
 * We use the _dt_*_ pattern to avoid clashes with any future macros glibc may
 * introduce, which have names of the pattern __attribute_blah__.
 */

#define _dt_constructor_(x) __attribute__((__constructor__(0)))
#define _dt_destructor_(x) __attribute__((__destructor__))
#define _dt_printflike_(string_index,first_to_check) __attribute__((__format__(__printf__,(string_index),(first_to_check))))
#define _dt_unused_ __attribute__((__unused__))
#define _dt_noreturn_ __attribute__((__noreturn__))
#define _dt_unlikely_(x) __builtin_expect((x),0)

#elif defined (__SUNPRO_C)

#define _dt_constructor_(x) _Pragma("init(" #x ")")
#define _dt_destructor_(x) _Pragma("fini(" #x ")")
#define _dt_noreturn_
#define _dt_unlikely_(x) (x)

/*
 * These are lint comments with no compiler equivalent.
 */

#define _dt_printflike_(string_index,first_to_check)
#define _dt_unused_

#endif
#endif
