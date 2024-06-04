/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'lockstat' SDT provider for DTrace-specific probes.
 */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

static const char		prvname[] = "lockstat";
static const char		modname[] = "vmlinux";

static probe_dep_t	probes[] = {
	{ "adaptive-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::mutex_lock" },
	{ "adaptive-acquire-error",
	   DTRACE_PROBESPEC_FUNC,	"fbt::*mutex_lock_killable" },
	{ "adaptive-acquire-error",
	   DTRACE_PROBESPEC_FUNC,	"fbt::*mutex_lock_interruptible" },
	{ "adaptive-block",
	   DTRACE_PROBESPEC_FUNC,	"fbt::mutex_lock" },
	{ "adaptive-block",
	   DTRACE_PROBESPEC_FUNC,	"fbt::schedule_preempt_disabled" },
	{ "adaptive-release",
	   DTRACE_PROBESPEC_FUNC,	"fbt::mutex_unlock" },
	{ "adaptive-spin",
	   DTRACE_PROBESPEC_FUNC,	"fbt::mutex_lock" },
	{ "adaptive-spin",
	   DTRACE_PROBESPEC_NAME,	"fbt::_raw_spin_lock:entry" },
	{ "rw-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_read_lock*" },
	{ "rw-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_read_trylock*" },
	{ "rw-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_write_lock*" },
	{ "rw-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_write_trylock*" },
	{ "rw-release",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_read_unlock*" },
	{ "rw-release",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_write_unlock*" },
	{ "rw-spin",
	   DTRACE_PROBESPEC_FUNC,	"fbt::queued_read_lock_slowpath" },
	{ "rw-spin",
	   DTRACE_PROBESPEC_FUNC,	"fbt::queued_write_lock_slowpath" },
	{ "spin-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_spin_lock*" },
	{ "spin-acquire",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_spin_trylock*" },
	{ "spin-release",
	   DTRACE_PROBESPEC_FUNC,	"fbt::_raw_spin_unlock*" },
	{ "spin-spin",
	   DTRACE_PROBESPEC_FUNC,	"fbt::queued_spin_lock_*" },
	{ "spin-spin",
	   DTRACE_PROBESPEC_FUNC,	"fbt::native_queued_spin_lock_*" },
	{ NULL, }
};

static probe_arg_t probe_args[] = {
	{ "adaptive-acquire", 0, { 0, 0, "struct mutex *" } },
	{ "adaptive-acquire-error", 0, { 0, 0, "struct mutex *" } },
	{ "adaptive-acquire-error", 1, { 1, 0, "int" } },
	{ "adaptive-block", 0, { 0, 0, "struct mutex *" } },
	{ "adaptive-block", 1, { 1, 0, "uint64_t" } },
	{ "adaptive-release", 0, { 0, 0, "struct mutex *" } },
	{ "adaptive-spin", 0, { 0, 0, "struct mutex *" } },
	{ "adaptive-spin", 1, { 1, 0, "uint64_t" } },
	{ "rw-acquire", 0, { 0, 0, "struct rwlock *" } },
	{ "rw-acquire", 1, { 1, 0, "int" } },
	{ "rw-release", 0, { 0, 0, "struct rwlock *" } },
	{ "rw-release", 1, { 1, 0, "int" } },
	{ "rw-spin", 0, { 0, 0, "struct rwlock *" } },
	{ "rw-spin", 1, { 1, 0, "uint64_t" } },
	{ "rw-spin", 2, { 2, 0, "int" } },
	{ "spin-acquire", 0, { 0, 0, "spinlock_t *" } },
	{ "spin-release", 0, { 0, 0, "spinlock_t *" } },
	{ "spin-spin", 0, { 0, 0, "spinlock_t *" } },
	{ "spin-spin", 1, { 1, 0, "uint64_t" } },
	{ NULL, }
};

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
};

/*
 * Provide all the "lockstat" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	/*
	 * Linux kernels earlier than 5.10.0 have a bug that can cause a kernel
	 * deadlock when placing a kretprobe on spinlock functions.
	 */
	if (dtp->dt_kernver < DT_VERSION_NUMBER(5, 10, 0))
		return 0;

	return dt_sdt_populate(dtp, prvname, modname, &dt_lockstat, &pattr,
			       probe_args, probes);
}

/*
 * Get a reference to the cpuinfo structure for the current CPU.
 *
 * Clobbers %r0 through %r5
 * Stores pointer to cpuinfo struct in %r6
 */
static void get_cpuinfo(dtrace_hdl_t *dtp, dt_irlist_t *dlp, uint_t exitlbl)
{
	dt_ident_t	*idp = dt_dlib_get_map(dtp, "cpuinfo");

	assert(idp != NULL);
	dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DT_TRAMP_SP_BASE));
	emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_2, 0, 0));
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
	emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, exitlbl));
	emit(dlp, BPF_MOV_REG(BPF_REG_6, BPF_REG_0));
}

/*
 * Copy the lock address from args[n] into the per-CPU cpuinfo structure
 * referenced by %r6.
 *
 * Clobbers %r1
 */
static void copy_lockaddr_into_cpuinfo(dt_irlist_t *dlp, int n)
{
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_7, DMST_ARG(n)));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_lock), BPF_REG_1));
}

/*
 * Copy the lock address from the per-CPU cpuinfo structure referenced by %r6
 * into args[n], and reset the lock address in the per-CPU cpuinfo structure
 * to 0.  If lbl is not DT_LBL_NONE, it will be used to label the instruction
 * that resets the lock address in cpuinfo.
 *
 * Clobbers %r1
 */
static void copy_lockaddr_from_cpuinfo(dt_irlist_t *dlp, int n, uint_t lbl)
{
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_lock)));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(n), BPF_REG_1));

	emitl(dlp, lbl,
		   BPF_STORE_IMM(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_lock), 0));
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_lockstat(void *data)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_probe_t	*prp = pcb->pcb_probe;
	dt_probe_t	*uprp = pcb->pcb_parent_probe;

	assert(uprp != NULL);

	get_cpuinfo(dtp, dlp, exitlbl);

	if (strcmp(prp->desc->prb, "adaptive-acquire") == 0 ||
	    strcmp(prp->desc->prb, "adaptive-release") == 0) {
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			copy_lockaddr_into_cpuinfo(dlp, 0);
			return 1;
		} else {
			copy_lockaddr_from_cpuinfo(dlp, 0, DT_LBL_NONE);
			return 0;
		}
	} else if (strcmp(prp->desc->prb, "adaptive-acquire-error") == 0) {
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			copy_lockaddr_into_cpuinfo(dlp, 0);
			return 1;
		} else {
			/*
			 * args[1] is already set by the underlying probe, but
			 * we only report the adaptive-acquire-error probe if
			 * the value is not 0.
			 */
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_7, DMST_ARG(1)));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_1, 0, exitlbl));
			copy_lockaddr_from_cpuinfo(dlp, 0, DT_LBL_NONE);
			return 0;
		}
	} else if (strcmp(prp->desc->prb, "adaptive-block") == 0) {
		/*
		 * - mutex_lock:entry inits lockstat_btime (0) and stores lock.
		 * - schedule_preempt_disabled:entry sets lockstat_bfrom
		 * - schedule_preempt_disabled:return increments lockstat_bfrom
		 * - mutex_lock:return sets the adaptive-block arguments
		 */
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			if (strcmp(uprp->desc->fun, "mutex_lock") == 0) {
				copy_lockaddr_into_cpuinfo(dlp, 0);

				/* Initialize lockstat_btime. */
				emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_btime), 0));
			} else {
				/* Store the start time. */
				emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
				emit(dlp, BPF_STORE(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_bfrom), BPF_REG_0));
			}

			return 1;
		} else {
			if (strcmp(uprp->desc->fun, "mutex_lock") != 0) {
				/* Increment the block time. */
				emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
				emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_bfrom)));
				emit(dlp, BPF_ALU64_REG(BPF_SUB, BPF_REG_0, BPF_REG_1));
				emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_btime), BPF_REG_0));

				return 1;
			} else {
				/*
				 * If lockstat_btime = 0, bail.
				 * Otherwise arg1 = lockstat_btime.
				 */
				emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_btime)));
				emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_1, 0, exitlbl));
				emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_1));

				copy_lockaddr_from_cpuinfo(dlp, 0, DT_LBL_NONE);
				return 0;
			}
		}
	} else if (strcmp(prp->desc->prb, "adaptive-spin") == 0) {
		/*
		 * - mutex_lock:entry stores lock and inits lockstat_stime (0).
		 * - _raw_spin_lock:entry sets lockstat_stime
		 * - mutex_lock:return sets the adaptive-spin arguments
		 */
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			if (strcmp(uprp->desc->fun, "mutex_lock") == 0) {
				copy_lockaddr_into_cpuinfo(dlp, 0);

				/* Initialize lockstat_stime. */
				emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime), 0));
			} else {
				/* Store the start time in lockstat_stime. */
				emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
				emit(dlp, BPF_STORE(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime), BPF_REG_0));
			}

			return 1;
		} else {
			/*
			 * If lockstat_stime is 0, bail.
			 * Otherwise, arg1 = time - lockstat_stime.
			 */
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime)));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_1, 0, exitlbl));
			emit(dlp, BPF_ALU64_REG(BPF_SUB, BPF_REG_0, BPF_REG_1));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));

			copy_lockaddr_from_cpuinfo(dlp, 0, DT_LBL_NONE);
			return 0;
		}
	} else if (strcmp(prp->desc->prb, "rw-acquire") == 0) {
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			copy_lockaddr_into_cpuinfo(dlp, 0);
			return 1;
		} else {
			int	kind = 1;	/* reader (default) */
			uint_t	lbl_reset = dt_irlist_label(dlp);

			if (strstr(uprp->desc->fun, "_write_") != NULL)
				kind = 0;	/* writer */

			if (strstr(uprp->desc->fun, "_trylock") != NULL) {
				/* The return value (arg1) must be 1. */
				emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_7, DMST_ARG(1)));
				emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_1, 1, lbl_reset));
			}

			/* Set arg1 = kind. */
			emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(1), kind));

			copy_lockaddr_from_cpuinfo(dlp, 0, lbl_reset);
			return 0;
		}
	} else if (strcmp(prp->desc->prb, "rw-spin") == 0) {
		/*
		 * - *_lock_slowpath:entry stores lock and sets lockstat_stime
		 * - *_lock_slowpath:return sets the rw-spin arguments
		 */
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			copy_lockaddr_into_cpuinfo(dlp, 0);

			/* Store the start time in lockstat_stime. */
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime), BPF_REG_0));

			return 1;
		} else {
			/*
			 * If lockstat_stime is 0, bail.
			 * Otherwise, arg1 = time - lockstat_stime.
			 */
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime)));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_1, 0, exitlbl));
			emit(dlp, BPF_ALU64_REG(BPF_SUB, BPF_REG_0, BPF_REG_1));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));

			copy_lockaddr_from_cpuinfo(dlp, 0, DT_LBL_NONE);
			return 0;
		}
	} else if (strcmp(prp->desc->prb, "spin-acquire") == 0) {
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			copy_lockaddr_into_cpuinfo(dlp, 0);
			return 1;
		} else {
			uint_t	lbl_reset = dt_irlist_label(dlp);

			if (strstr(uprp->desc->fun, "_trylock") != NULL) {
				/* The return value (arg1) must be 1. */
				emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_7, DMST_ARG(1)));
				emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_1, 1, lbl_reset));
			}

			copy_lockaddr_from_cpuinfo(dlp, 0, lbl_reset);
			return 0;
		}
	} else if (strcmp(prp->desc->prb, "spin-spin") == 0) {
		/*
		 * - *_lock_slowpath:entry stores lock and sets lockstat_stime
		 * - *_lock_slowpath:return sets the rw-spin arguments
		 */
		if (strcmp(uprp->desc->prb, "entry") == 0) {
			copy_lockaddr_into_cpuinfo(dlp, 0);

			/* Store the start time in lockstat_stime. */
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime), BPF_REG_0));

			return 1;
		} else {
			/*
			 * If lockstat_stime is 0, bail.
			 * Otherwise, arg1 = time - lockstat_stime.
			 */
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_6, offsetof(dt_bpf_cpuinfo_t, lockstat_stime)));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_1, 0, exitlbl));
			emit(dlp, BPF_ALU64_REG(BPF_SUB, BPF_REG_0, BPF_REG_1));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));

			copy_lockaddr_from_cpuinfo(dlp, 0, DT_LBL_NONE);
			return 0;
		}
	}

	return 0;
}

dt_provimpl_t	dt_lockstat = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
