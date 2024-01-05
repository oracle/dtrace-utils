/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'proc' SDT provider for DTrace specific probes.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bpf_asm.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_bpf.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "proc";
static const char		modname[] = "vmlinux";

/*
 * The proc-provider probes make use of probes that are already provided by
 * other providers.  As such, the proc probes are 'dependent probes' because
 * they depend on underlying probes to get triggered and they also depend on
 * argument data provided by the underlying probe to manufacture their own
 * arguments.
 *
 * As a type of SDT probes, proc probes are defined with a signature (list of
 * arguments - possibly empty) that may use translator support to provide the
 * actual argument values.  Therefore, obtaining the value of arguments for
 * a proc probe goes through two layers of processing:
 *
 *  (1) the arguments of the underlying probe are reworked to match the
 *	expected layout of raw arguments for the proc probe
 *  (2) an argument mapping table (and supporting translators) is used to get
 *	the value of an arguument based on the raw variable data of the proc
 *	probe
 *
 * To accomplish this, proc probes generate a trampoline that rewrites the
 * arguments of the underlying probe.  (The dependent probe support code in the
 * underlying probe saves the arguments of the underying probe in the mstate
 * before executing the trampoline and clauses of the dependent probe, and it
 * restores them afterwards in case there are multiple dependent probes.)
 *
 * Because proc probes dependent on an underlying probe that may be too generic
 * (e.g. proc:::exec-success depending on syscall::execve*:return), the
 * trampoline code can include a pre-condition (much like a predicate) that can
 * bypass execution unless the condition is met (e.g. proc:::exec-success
 * requires syscall::execve*:return's arg1 to be 0).
 *
 * FIXME:
 * The dependent probe support should include a priority specification to drive
 * the order in which dependent probes are added to the underlying probe.  This
 * is needed to enforce specific probe firing semantics (e.g. proc:::start must
 * always precede [roc:::lwp-start).
 */

typedef struct probe_arg {
	const char	*name;			/* name of probe */
	int		argno;			/* argument number */
	dt_argdesc_t	argdesc;		/* argument description */
} probe_arg_t;

/*
 * Probe signature specifications
 *
 * This table *must* group the arguments of probes.  I.e. the arguments of a
 * given probe must be listed in consecutive records.
 * A single probe entry that mentions only name of the probe indicates a probe
 * that provides no arguments.
 */
static probe_arg_t probe_args[] = {
	{ "create", 0, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "exec", 0, { 0, DT_NF_USERLAND, "string", } },
	{ "exec-failure", 0, { 0, 0, "int", } },
	{ "exec-success", },
	{ "exit", 0, { 0, 0, "int", } },
	{ "lwp-create", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "lwp-create", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "lwp-exit", },
	{ "lwp-start", },
	{ "signal-clear", 0, { 0, 0, "int", } },
	{ "signal-discard", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "signal-discard", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "signal-discard", 2, { 1, 0, "int", } },
	{ "signal-handle", 0, { 0, 0, "int", } },
	{ "signal-handle", 1, { 1, 0, "siginfo_t *", } },
	{ "signal-handle", 2, { 2, 0, "void (*)(void)", } },
	{ "signal-send", 0, { 0, 0, "struct task_struct *", "lwpsinfo_t *" } },
	{ "signal-send", 1, { 0, 0, "struct task_struct *", "psinfo_t *" } },
	{ "signal-send", 2, { 1, 0, "int", } },
	{ "start", },
};

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
};

/*
 * Provide all the "proc" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	int		i;
	int		n = 0;

	prv = dt_provider_create(dtp, prvname, &dt_proc, &pattr, NULL);
	if (prv == NULL)
		return -1;			/* errno already set */

	/*
	 * Create "proc" probes based on the probe_args list.  Since each probe
	 * will have at least one entry (with argno == 0), we can use those
	 * entries to identify the probe names.
	 */
	for (i = 0; i < ARRAY_SIZE(probe_args); i++) {
		probe_arg_t	*arg = &probe_args[i];

		if (arg->argno == 0 &&
		    dt_probe_insert(dtp, prv, prvname, modname, "", arg->name,
				    NULL))
			n++;
	}

	return n;
}

static void enable(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	dt_probe_t		*uprp = NULL;
	dtrace_probedesc_t	pd;

	if (strcmp(prp->desc->prb, "create") == 0 ||
	    strcmp(prp->desc->prb, "lwp-create") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "rawtp";
		pd.mod = "sched";
		pd.fun = "";
		pd.prb = "sched_process_fork";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "exec") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "syscall";
		pd.mod = "";
		pd.fun = "execve";
		pd.prb = "entry";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);

		pd.fun = "execveat";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "exec-failure") == 0 ||
		   strcmp(prp->desc->prb, "exec-success") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "syscall";
		pd.mod = "";
		pd.fun = "execve";
		pd.prb = "return";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);

		pd.fun = "execveat";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "exit") == 0 ||
		   strcmp(prp->desc->prb, "lwp-exit") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "rawtp";
		pd.mod = "";
		pd.fun = "";
		pd.prb = "sched_process_exit";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "signal-clear") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "syscall";
		pd.mod = "";
		pd.fun = "rt_sigtimedwait";
		pd.prb = "return";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "signal-discard") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "rawtp";
		pd.mod = "signal";
		pd.fun = "";
		pd.prb = "signal_generate";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "signal-handle") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "rawtp";
		pd.mod = "signal";
		pd.fun = "";
		pd.prb = "signal_deliver";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "signal-send") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "fbt";
		pd.mod = "";
		pd.fun = "complete_signal";
		pd.prb = "entry";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	} else if (strcmp(prp->desc->prb, "start") == 0 ||
		   strcmp(prp->desc->prb, "lwp-start") == 0) {
		pd.id = DTRACE_IDNONE;
		pd.prv = "fbt";
		pd.mod = "";
		pd.fun = "schedule_tail";
		pd.prb = "return";

		uprp = dt_probe_lookup(dtp, &pd);
		assert(uprp != NULL);

		dt_probe_add_dependent(dtp, uprp, prp);
		dt_probe_enable(dtp, uprp);
	}

	/*
	 * Finally, ensure we're in the list of enablings as well.
	 * (This ensures that, among other things, the probes map
	 * gains entries for us.)
	 */
	if (!dt_in_list(&dtp->dt_enablings, prp))
		dt_list_append(&dtp->dt_enablings, prp);
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_proc(void *data)
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

	if (strcmp(prp->desc->prb, "create") == 0) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "exec-success") == 0) {
		/* Pre-condition: exec syscall returns 0 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, exitlbl));
	} else if (strcmp(prp->desc->prb, "exec-failure") == 0) {
		/* Pre-condition: exec syscall returns non-0 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, exitlbl));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "exit") == 0) {
		ctf_file_t	*cfp = dtp->dt_shared_ctf;
		ctf_id_t	type;
		ctf_membinfo_t	ctm;
		int		rc = 0;
		uint_t		lbl_out = dt_irlist_label(dlp);

		/* Only fire this probe for the task group leader. */
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
		emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_0));
		emit(dlp,  BPF_ALU64_IMM(BPF_LSH, BPF_REG_0, 32));
		emit(dlp,  BPF_ALU64_IMM(BPF_RSH, BPF_REG_0, 32));
		emit(dlp,  BPF_ALU64_IMM(BPF_RSH, BPF_REG_1, 32));
		emit(dlp,  BPF_BRANCH_REG(BPF_JNE, BPF_REG_0, BPF_REG_1, exitlbl));

		if (!cfp)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOCTF);
		type = ctf_lookup_by_name(cfp, "struct task_struct");
		if (type == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(cfp);
			longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
		}
		rc = ctf_member_info(cfp, type, "exit_code", &ctm);
		if (rc == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(cfp);
			longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
		}

		/*
		 * This implements:
		 *	%r1 = tsk->exit_code
		 *	if ((%r1 & 0x7f) == 0) args[0] = 1;	// CLD_EXITED
		 *	else if (%r1 & 0x80) args[0] = 3;	// CLD_DUMPED
		 *	else args[0] = 2;			// CLD_KILLED
		 */
		emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_STK_SPILL(0)));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(int)));
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, ctm.ctm_offset / NBBY));
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_1, BPF_REG_FP, DT_STK_SPILL(0)));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_0, 1));
		emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_1));
		emit(dlp,  BPF_ALU64_IMM(BPF_AND, BPF_REG_2, 0x7f));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_2, 0, lbl_out));

		emit(dlp,  BPF_MOV_IMM(BPF_REG_0, 3));
		emit(dlp,  BPF_ALU64_IMM(BPF_AND, BPF_REG_1, 0x80));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_1, 0, lbl_out));

		emit(dlp,  BPF_MOV_IMM(BPF_REG_0, 2));
		emitl(dlp, lbl_out,
			   BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "lwp-create") == 0) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "signal-clear") == 0) {
		/* Pre-condition: arg0 > 0 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(0)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JLE, BPF_REG_0, 0, exitlbl));
	} else if (strcmp(prp->desc->prb, "signal-discard") == 0) {
		/* Pre-condition: arg4 == 1 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(4)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 1, exitlbl));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_7, DMST_ARG(0)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(2)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	} else if (strcmp(prp->desc->prb, "signal-handle") == 0) {
		int	off;
		size_t	sz;
		uint_t	lbl_keep = dt_irlist_label(dlp);

		/*
		 * Getting the signal number right is a bit tricky because any
		 * unhandled signal triggers a SIGKILL to be delivered (to kill
		 * the task) while reporting the actual signal number in the
		 * signal struct for the task.  The real signal number is then
		 * to be found in task->signal->group_exit_code.
		 *
		 * So. if arg0 is SIGKILL, we look at group_exit_code, and if
		 * it is > 0, use that value as signal number.
		 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(0)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, SIGKILL, lbl_keep));

		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_task));
		off = dt_cg_ctf_offsetof("struct task_struct", "signal", &sz, 0);
		emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_STK_SPILL(0)));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, sz));
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_FP, DT_STK_SPILL(0)));
		off = dt_cg_ctf_offsetof("struct signal_struct", "group_exit_code", &sz, 0);
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_STK_SPILL(0)));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, sz));
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp, BPF_LOAD(BPF_W, BPF_REG_0, BPF_REG_FP, DT_STK_SPILL(0)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_keep));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));

		emitl(dlp, lbl_keep,
			   BPF_NOP());

		/* All three arguments are in their proper place. */
	} else if (strcmp(prp->desc->prb, "signal-send") == 0) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_7, DMST_ARG(0)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_1));
	}

	return 0;
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	int		i;
	int		pidx = -1;
	int		argc = 0;
	dt_argdesc_t	*argv = NULL;

	for (i = 0; i < ARRAY_SIZE(probe_args); i++) {
		probe_arg_t	*arg = &probe_args[i];

		if (strcmp(arg->name, prp->desc->prb) == 0) {
			if (pidx == -1) {
				pidx = i;

				if (arg->argdesc.native == NULL)
					break;
			}

			argc++;
		}
	}

	if (argc == 0)
		goto done;

	argv = dt_zalloc(dtp, argc * sizeof(dt_argdesc_t));
	if (!argv)
		return -ENOMEM;

	for (i = pidx; i < pidx + argc; i++) {
		probe_arg_t	*arg = &probe_args[i];

		argv[arg->argno] = arg->argdesc;
	}

done:
	*argcp = argc;
	*argvp = argv;

	return 0;
}

dt_provimpl_t	dt_proc = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &enable,
	.trampoline	= &trampoline,
	.probe_info	= &probe_info,
};
