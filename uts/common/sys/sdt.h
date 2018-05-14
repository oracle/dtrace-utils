/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _SYS_SDT_H_
#define	_SYS_SDT_H_

#ifdef	__cplusplus
extern "C" {
#endif

#if (__STDC_VERSION__ >= 199901L && __GNUC__ && !__STRICT_ANSI__) || defined(__cplusplus)

#define __stringify_1(...)	# __VA_ARGS__
#define __stringify(...)	__stringify_1(__VA_ARGS__)
#include <sys/sdt_internal.h>

#define __DTRACE_TYPE_UNSIGNED_LONG_EACH(x) unsigned long
#define __DTRACE_ULONG_CAST_EACH(x) (unsigned long)x

#define	DTRACE_PROBE(provider, name, ...)	{                       \
	extern void __dtrace_##provider##___##name(__DTRACE_APPLY_DEFAULT(__DTRACE_TYPE_UNSIGNED_LONG_EACH, void, ## __VA_ARGS__)); \
	__dtrace_##provider##___##name(__DTRACE_APPLY(__DTRACE_ULONG_CAST_EACH, ## __VA_ARGS__)); \
}

#else

#define	DTRACE_PROBE(provider, name) {					\
	extern void __dtrace_##provider##___##name(void);		\
	__dtrace_##provider##___##name();				\
}

#endif

/*
 * For backward compatibility and pre-C99 compilers.
 */

#define	DTRACE_PROBE1(provider, name, arg1) {				\
	extern void __dtrace_##provider##___##name(unsigned long);	\
	__dtrace_##provider##___##name((unsigned long)arg1);		\
}

#define	DTRACE_PROBE2(provider, name, arg1, arg2) {			\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long);						\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2);					\
}

#define	DTRACE_PROBE3(provider, name, arg1, arg2, arg3) {		\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long);				\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3);			\
}

#define	DTRACE_PROBE4(provider, name, arg1, arg2, arg3, arg4) {		\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long);		\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4);					\
}

#define	DTRACE_PROBE5(provider, name, arg1, arg2, arg3, arg4, arg5) {	\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long);\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5);			\
}

#define	DTRACE_PROBE6(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6) {								\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long);						\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5,			\
	    (unsigned long)arg6);					\
}

#define	DTRACE_PROBE7(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6, arg7) {							\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long);				\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5,			\
	    (unsigned long)arg6, (unsigned long)arg7);			\
}

#define	DTRACE_PROBE8(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6, arg7, arg8) {						\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long, unsigned long);		\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5,			\
	    (unsigned long)arg6, (unsigned long)arg7,			\
	    (unsigned long)arg8);					\
}

#define	DTRACE_PROBE9(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6, arg7, arg8, arg9) {					\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long);\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5,			\
	    (unsigned long)arg6, (unsigned long)arg7,			\
	    (unsigned long)arg8, (unsigned long)arg9);			\
}

#define	DTRACE_PROBE10(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6, arg7, arg8, arg9, arg10) {				\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long);						\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2, (unsigned long)arg3,			\
	    (unsigned long)arg4, (unsigned long)arg5,			\
	    (unsigned long)arg6, (unsigned long)arg7,			\
	    (unsigned long)arg8, (unsigned long)arg9,			\
	    (unsigned long)arg10);					\
}

#define	DTRACE_PROBE11(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6, arg7, arg8, arg9, arg10, arg11) {				\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long);				\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2,  (unsigned long)arg3,			\
	    (unsigned long)arg4,  (unsigned long)arg5,			\
	    (unsigned long)arg6,  (unsigned long)arg7,			\
	    (unsigned long)arg8,  (unsigned long)arg9,			\
	    (unsigned long)arg10, (unsigned long)arg11);		\
}

#define	DTRACE_PROBE12(provider, name, arg1, arg2, arg3, arg4, arg5,	\
	arg6, arg7, arg8, arg9, arg10, arg11, arg12) {			\
	extern void __dtrace_##provider##___##name(unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long, unsigned long, unsigned long,	\
	    unsigned long, unsigned long, unsigned long);		\
	__dtrace_##provider##___##name((unsigned long)arg1,		\
	    (unsigned long)arg2,  (unsigned long)arg3,			\
	    (unsigned long)arg4,  (unsigned long)arg5,			\
	    (unsigned long)arg6,  (unsigned long)arg7,			\
	    (unsigned long)arg8,  (unsigned long)arg9,			\
	    (unsigned long)arg10, (unsigned long)arg11,			\
	    (unsigned long)arg12);					\
}

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SDT_H_ */
