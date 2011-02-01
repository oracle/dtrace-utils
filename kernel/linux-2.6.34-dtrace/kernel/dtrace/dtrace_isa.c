/*
 * FILE:	dtrace_isa.c
 * DESCRIPTION:	Dynamic Tracing: architecture specific support functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include "dtrace.h"

/* FIXME */
uintptr_t _userlimit = 0xffffffffL;
uintptr_t kernelbase = 0xffffffffL;

cpu_core_t	cpu_core[NR_CPUS];

extern void	dtrace_copy(uintptr_t, uintptr_t, size_t);
extern void	dtrace_copystr(uintptr_t, uintptr_t, size_t,
			       volatile uint16_t *);

static int dtrace_copycheck(uintptr_t uaddr, uintptr_t kaddr, size_t size)
{
	ASSERT(kaddr >= kernelbase && kaddr + size >= kaddr);

	if (uaddr + size >= kernelbase || uaddr + size < uaddr) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[smp_processor_id()].cpuc_dtrace_illval = uaddr;
		return 0;
	}

	return 1;
}

void dtrace_copyin(uintptr_t uaddr, uintptr_t kaddr, size_t size,
		   volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copy(uaddr, kaddr, size);
}

void dtrace_copyout(uintptr_t uaddr, uintptr_t kaddr, size_t size,
		    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copy(kaddr, uaddr, size);
}

void dtrace_copyinstr(uintptr_t uaddr, uintptr_t kaddr, size_t size,
		      volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copystr(uaddr, kaddr, size, flags);
}

void dtrace_copyoutstr(uintptr_t uaddr, uintptr_t kaddr, size_t size,
		       volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copystr(kaddr, uaddr, size, flags);
}

#define DTRACE_FUWORD(bits) \
	uint##bits##_t dtrace_fuword##bits(void *uaddr)			      \
	{								      \
		extern uint##bits##_t	dtrace_fuword##bits##_nocheck(void *);\
									      \
		if ((uintptr_t)uaddr >= _userlimit) {			      \
			DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);		      \
			cpu_core[smp_processor_id()].cpuc_dtrace_illval =     \
							(uintptr_t)uaddr;     \
			return 0;					      \
		}							      \
									      \
		return dtrace_fuword##bits##_nocheck(uaddr);		      \
	}

DTRACE_FUWORD(8)
DTRACE_FUWORD(16)
DTRACE_FUWORD(32)
DTRACE_FUWORD(64)
