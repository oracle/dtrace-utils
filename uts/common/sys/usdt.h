/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _SYS_USDT_H_
#define	_SYS_USDT_H_

#ifdef	__cplusplus
extern "C" {
#endif

#define __stringify_1(...)	# __VA_ARGS__
#define __stringify(...)	__stringify_1(__VA_ARGS__)

#include "usdt_gennote.h"

#define __DT_TYPE_UNSIGNED_LONG_EACH(n, x) unsigned long
#define __DT_ULONG_CAST_EACH(n, x) (unsigned long)x

#define	DTRACE_PROBE(provider, name, ...) \
		_USDT_PROBE(provider, name, ## __VA_ARGS__)

/* For backward compatibility and pre-C99 compilers. */

#define	DTRACE_PROBE1(provider, name, arg1)				\
		DTRACE_PROBE(provider, name, arg1)

#define	DTRACE_PROBE2(provider, name, arg1, arg2)			\
		DTRACE_PROBE(provider, name, arg1, arg2)

#define	DTRACE_PROBE3(provider, name, arg1, arg2, arg3)			\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3)

#define	DTRACE_PROBE4(provider, name, arg1, arg2, arg3, arg4)		\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4)

#define	DTRACE_PROBE5(provider, name, arg1, arg2, arg3, arg4, arg5)	\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4, arg5)

#define	DTRACE_PROBE6(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		      arg6)						\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6)

#define	DTRACE_PROBE7(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		     arg6, arg7)					\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6, arg7)

#define	DTRACE_PROBE8(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		      arg6, arg7, arg8)					\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6, arg7, arg8)

#define	DTRACE_PROBE9(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		      arg6, arg7, arg8, arg9)				\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6, arg7, arg8, arg9)

#define	DTRACE_PROBE10(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		       arg6, arg7, arg8, arg9, arg10)			\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6, arg7, arg8, arg9, arg10)

#define	DTRACE_PROBE11(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		       arg6, arg7, arg8, arg9, arg10, arg11)		\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6, arg7, arg8, arg9, arg10, arg11)

#define	DTRACE_PROBE12(provider, name, arg1, arg2, arg3, arg4, arg5,	\
		       arg6, arg7, arg8, arg9, arg10, arg11, arg12)	\
		DTRACE_PROBE(provider, name, arg1, arg2, arg3, arg4,	\
			     arg5, arg6, arg7, arg8, arg9, arg10,	\
			     arg11, arg12)

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_USDT_H_ */
