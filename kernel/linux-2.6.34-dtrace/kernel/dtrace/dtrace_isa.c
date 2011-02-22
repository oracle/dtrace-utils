/*
 * FILE:	dtrace_isa.c
 * DESCRIPTION:	Dynamic Tracing: architecture specific support functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/hardirq.h>
#include <linux/smp.h>

#include "dtrace.h"

/* FIXME */
uintptr_t _userlimit = 0xffffffffL;
uintptr_t kernelbase = 0xffffffffL;

cpu_core_t	cpu_core[NR_CPUS];
DEFINE_MUTEX(cpu_lock);

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

struct frame {
	struct frame	*fr_savfp;
	unsigned long	fr_savpc;
} __attribute__((packed));

static void dtrace_invop_callsite(void)
{
}

uint64_t dtrace_getarg(int arg, int aframes)
{
	struct frame	*fp = (struct frame *)dtrace_getfp();
	uintptr_t	*stack;
	int		i;
	uint64_t	val;
#ifdef __i386__
	int		regmap[] = {
					REG_EAX,
					REG_EDX,
					REG_ECX
				   };
#else
	int		regmap[] = {
					REG_RDI,
					REG_RSI,
					REG_RDX,
					REG_RCX,
					REG_R8,
					REG_R9
				   };
#endif
	int		nreg = sizeof(regmap) / sizeof(regmap[0]);

	for (i = 1; i <= aframes; i++) {
		fp = fp->fr_savfp;

		if (fp->fr_savpc == (uintptr_t)dtrace_invop_callsite) {
#ifdef __i386__
			/* FIXME */
#else
			/* FIXME */
#endif

			goto load;
		}
	}

	/*
	 * We know that we did not get here through a trap to get into the
	 * dtrace_probe() function, so this was a straight call into it from
	 * a provider.  In that case, we need to shift the argument that we
	 * are looking for, because the probe ID will be the first argument to
	 * dtrace_probe().
	 */
	arg++;

#ifndef __i386__
	if (arg <= nreg) {
		/*
		 * This should not happen.  If the argument was passed in a
		 * register then it should have been, ...passed in a reguster.
		 */
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return 0;
	}

	arg -= nreg + 1;
#endif

	stack = (uintptr_t *)&fp[1];

load:
	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	val = stack[arg];
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);

	return val;
}

int dtrace_getipl(void)
{
	return in_interrupt();
}

ulong_t dtrace_getreg(struct pt_regs *rp, uint_t reg)
{
#ifdef __i386__
	if (reg > SS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return 0;
	}

	switch (reg) {
	case REG_GS:
	case REG_FS:
	case REG_ES:
	case REG_DS:
	case REG_CS:
		return rp->cs;
	case REG_EDI:
		return rp->di;
	case REG_ESI:
		return rp->si;
	case REG_EBP:
		return rp->bp;
	case REG_ESP:
	case REG_UESP:
		return rp->sp;
	case REG_EBX:
		return rp->bx;
	case REG_EDX:
		return rp->dx;
	case REG_ECX:
		return rp->cx;
	case REG_EAX:
		return rp->ax;
	case REG_TRAPNO:
		return rp->orig_ax;
	case REG_ERR:
		return rp->di;
	case REG_EIP:
		return rp->ip;
	case REG_EFL:
		return rp->flags;
	case REG_SS:
		return rp->ss;
	default:
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return 0;
	}
#else
	int	regmap[] = {
				REG_GS,		/*  0 -> GS */
				REG_FS,		/*  1 -> FS */
				REG_ES,		/*  2 -> ES */
				REG_DS,		/*  3 -> DS */
				REG_RDI,	/*  4 -> EDI */
				REG_RSI,	/*  5 -> ESI */
				REG_RBP,	/*  6 -> EBP */
				REG_RSP,	/*  7 -> ESP */
				REG_RBX,	/*  8 -> EBX */
				REG_RDX,	/*  9 -> EDX */
				REG_RCX,	/* 10 -> ECX */
				REG_RAX,	/* 11 -> EAX */
				REG_TRAPNO,	/* 12 -> TRAPNO */
				REG_ERR,	/* 13 -> ERR */
				REG_RIP,	/* 14 -> EIP */
				REG_CS,		/* 15 -> CS */
				REG_RFL,	/* 16 -> EFL */
				REG_RSP,	/* 17 -> UESP */
				REG_SS,		/* 18 -> SS */
			   };

	if (reg <= SS) {
		if (reg >= sizeof(regmap) / sizeof(int)) {
			DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
			return 0;
		}

		reg = regmap[reg];
	} else
		reg -= SS + 1;

	switch (reg) {
	case REG_RDI:
		return rp->di;
	case REG_RSI:
		return rp->si;
	case REG_RDX:
		return rp->dx;
	case REG_RCX:
		return rp->cx;
	case REG_R8:
		return rp->r8;
	case REG_R9:
		return rp->r9;
	case REG_RAX:
		return rp->ax;
	case REG_RBX:
		return rp->bx;
	case REG_RBP:
		return rp->bp;
	case REG_R10:
		return rp->r10;
	case REG_R11:
		return rp->r11;
	case REG_R12:
		return rp->r12;
	case REG_R13:
		return rp->r13;
	case REG_R14:
		return rp->r14;
	case REG_R15:
		return rp->r15;
	case REG_CS:
	case REG_DS:
	case REG_ES:
	case REG_FS:
	case REG_GS:
		return rp->cs;
	case REG_TRAPNO:
		return rp->orig_ax;
	case REG_ERR:
		return rp->di;
	case REG_RIP:
		return rp->ip;
	case REG_SS:
		return rp->ss;
	case REG_RFL:
		return rp->flags;
	case REG_RSP:
		return rp->sp;
	default:
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return 0;
	}
#endif
}

static void dtrace_sync_func(void)
{
}

void dtrace_sync(void)
{
	dtrace_xcall(DTRACE_CPUALL, (dtrace_xcall_t)dtrace_sync_func, NULL);
}

void dtrace_xcall(processorid_t cpu, dtrace_xcall_t func, void *arg)
{
	if (cpu == DTRACE_CPUALL) {
		preempt_disable();
		smp_call_function(func, arg, 1);
		preempt_enable();
	} else
		smp_call_function_single(cpu, func, arg, 1);
}

void dtrace_toxic_ranges(void (*func)(uintptr_t, uintptr_t))
{
	/* FIXME */
}

ktime_t dtrace_gethrestime(void)
{
	return ktime_get();
}
