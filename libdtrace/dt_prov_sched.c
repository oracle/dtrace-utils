/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'sched' SDT provider for DTrace-specific probes.
 */
#include <assert.h>
#include <errno.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

static const char		prvname[] = "sched";
static const char		modname[] = "vmlinux";

static probe_dep_t	probes[] = {
	{ "dequeue",
	  DTRACE_PROBESPEC_NAME,	"fbt::dequeue_task_*:entry" },
	{ "enqueue",
	  DTRACE_PROBESPEC_NAME,	"fbt::enqueue_task_*:entry" },
	{ "off-cpu",
	  DTRACE_PROBESPEC_NAME,	"rawtp:sched::sched_switch" },
	{ "on-cpu",
	  DTRACE_PROBESPEC_NAME,	"fbt::schedule_tail:entry" },
	{ "surrender",
	  DTRACE_PROBESPEC_NAME,	"fbt::do_sched_yield:entry" },
	{ "tick",
	  DTRACE_PROBESPEC_NAME,	"fbt::scheduler_tick:entry" },
	{ "wakeup",
	  DTRACE_PROBESPEC_NAME,	"rawtp:sched::sched_wakeup" },
	{ NULL, }
};

static probe_arg_t probe_args[] = {
#if 0
	{ "change-pri", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "change-pri", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "change-pri", 2, { 1, 0, "int", } },
#endif
	{ "dequeue", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "dequeue", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "dequeue", 2, { 1, 0, "cpuinfo_t *", } },
	{ "dequeue", 3, { 2, 0, "int", } },
	{ "enqueue", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "enqueue", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "enqueue", 2, { 1, 0, "cpuinfo_t *", } },
	{ "off-cpu", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "off-cpu", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "on-cpu", },
#if 0
	{ "preempt", },
	{ "remain-cpu", },
	{ "sleep", },
#endif
	{ "surrender", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "surrender", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "tick", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "tick", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "wakeup", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "wakeup", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
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
 * Provide all the "sched" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	return dt_sdt_populate(dtp, prvname, modname, &dt_sched, &pattr,
			       probe_args, probes);
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_sched(void *data)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_probe_t	*prp = pcb->pcb_probe;

	if (strcmp(prp->desc->prb, "dequeue") == 0) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
		/*
		 * FIXME: arg1 should be a pointer to cpuinfo_t for the CPU
		 *	  associated with the runqueue.
		 */
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(1), 0));
	} else if (strcmp(prp->desc->prb, "enqueue") == 0) {
/*
 * This is ugly but necessary...  enqueue_task() takes a flags argument and the
 * ENQUEUE_HEAD flag is used to indicate that the task is to be placed at the
 * head of the queue.  We need to be able to pass this special case as arg2
 * in the enqueue probe.
 *
 * The flag values are found in kernel/sched/sched.h which is not exposed
 * outside the kernel source tree.
 */
#define ENQUEUE_HEAD	0x10

		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
		/*
		 * FIXME: arg1 should be a pointer to cpuinfo_t for the CPU
		 *	  associated with the runqueue.
		 */
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(1), 0));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(2)));
		emit(dlp, BPF_ALU64_IMM(BPF_AND, BPF_REG_0, ENQUEUE_HEAD));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "off-cpu") == 0) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(2)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "surrender") == 0 ||
		   strcmp(prp->desc->prb, "tick") == 0) {
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_task));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	}

	return 0;
}

dt_provimpl_t	dt_sched = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
