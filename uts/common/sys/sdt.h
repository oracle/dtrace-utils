/* Copyright (C) 2011, 2012, 2013 Oracle, Inc. */

#ifndef _SYS_SDT_H_
#define	_SYS_SDT_H_

#ifdef	__cplusplus
extern "C" {
#endif

#define	DTRACE_PROBE(provider, name) {					\
	extern void __dtrace_##provider##___##name(void);		\
	__dtrace_##provider##___##name();				\
}

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

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SDT_H_ */
