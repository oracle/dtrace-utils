/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'proc' SDT provider for DTrace specific probes.
 */
#include <assert.h>
#include <errno.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

static const char		prvname[] = "proc";
static const char		modname[] = "vmlinux";

static probe_dep_t	probes[] = {
	{ "create",
	  DTRACE_PROBESPEC_NAME,	"rawtp:sched::sched_process_fork" },
	{ "exec",
	  DTRACE_PROBESPEC_NAME,	"syscall::execve*:entry" },
	{ "exec-failure",
	  DTRACE_PROBESPEC_NAME,	"syscall::execve*:return" },
	{ "exec-success",
	  DTRACE_PROBESPEC_NAME,	"syscall::execve*:return" },
	{ "exit",
	  DTRACE_PROBESPEC_NAME,	"rawtp:sched::sched_process_exit" },
	{ "lwp-create",
	  DTRACE_PROBESPEC_NAME,	"rawtp:sched::sched_process_fork" },
	{ "lwp-exit",
	  DTRACE_PROBESPEC_NAME,	"rawtp:sched::sched_process_exit" },
	{ "lwp-start",
	  DTRACE_PROBESPEC_NAME,	"fbt::schedule_tail:return" },
	{ "signal-clear",
	  DTRACE_PROBESPEC_NAME,	"syscall::rt_sigtimedwait:return" },
	{ "signal-discard",
	  DTRACE_PROBESPEC_NAME,	"rawtp:signal::signal_generate" },
	{ "signal-handle",
	  DTRACE_PROBESPEC_NAME,	"rawtp:signal::signal_deliver" },
	{ "signal-send",
	  DTRACE_PROBESPEC_NAME,	"fbt::complete_signal:entry" },
	{ "start",
	  DTRACE_PROBESPEC_NAME,	"fbt::schedule_tail:return" },
	{ NULL, }
};

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
	{ NULL, },
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
	return dt_sdt_populate(dtp, prvname, modname, &dt_proc, &pattr,
			       probe_args, probes);
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
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_TRAMP_SP_SLOT(0)));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(int)));
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_7, DMST_ARG(0)));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, ctm.ctm_offset / NBBY));
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_1, BPF_REG_FP, DT_TRAMP_SP_SLOT(0)));
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
		off = dt_cg_ctf_offsetof("struct task_struct", "signal", &sz,
					 NULL, 0);
		emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_TRAMP_SP_SLOT(0)));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, sz));
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_FP, DT_TRAMP_SP_SLOT(0)));
		off = dt_cg_ctf_offsetof("struct signal_struct",
					 "group_exit_code", &sz, NULL, 0);
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_TRAMP_SP_SLOT(0)));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, sz));
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp, BPF_LOAD(BPF_W, BPF_REG_0, BPF_REG_FP, DT_TRAMP_SP_SLOT(0)));
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

dt_provimpl_t	dt_proc = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
