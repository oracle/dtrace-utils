/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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

/*
 * We need a very low priority number to ensure that the constructor
 * for USDT DOF loading is executed as early as possible.  Meanwhile,
 * constructor priorities from 0 to 100 are reserved for the implementation.
 * So the constructor priority may trigger a gcc compiler warning.
 */
#define _dt_constructor_(x) __attribute__((__constructor__(0)))
#define _dt_destructor_(x) __attribute__((__destructor__))
#define _dt_printflike_(string_index,first_to_check) __attribute__((__format__(__printf__,(string_index),(first_to_check))))
#define _dt_unused_ __attribute__((__unused__))
#define _dt_noreturn_ __attribute__((__noreturn__))
#define _dt_weak_ __attribute__((__weak__))
#define _dt_unlikely_(x) __builtin_expect((x),0)
#define _dt_barrier_(x) __asm__ __volatile__("": :"r"(x):"memory")

#elif defined (__SUNPRO_C)

#define _dt_constructor_(x) _Pragma("init(" #x ")")
#define _dt_destructor_(x) _Pragma("fini(" #x ")")
#define _dt_noreturn_
#define _dt_weak_
#define _dt_unlikely_(x) (x)
#define _dt_barrier_(x)

/*
 * These are lint comments with no compiler equivalent.
 */

#define _dt_printflike_(string_index,first_to_check)
#define _dt_unused_

#endif
#endif
