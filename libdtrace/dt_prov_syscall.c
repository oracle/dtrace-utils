/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The syscall provider for DTrace.
 *
 * System call probes are exposed by the kernel as tracepoint events in the
 * "syscalls" group.  Entry probe names start with "sys_enter_" and exit probes
 * start with "sys_exit_".
 *
 * Mapping from event name to DTrace probe name:
 *
 *	syscalls:sys_enter_<name>		syscall:vmlinux:<name>:entry
 *	syscalls:sys_exit_<name>		syscall:vmlinux:<name>:return
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/bpf.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bpf_asm.h>

#include "dt_impl.h"
#include "dt_bpf_builtins.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_pt_regs.h"

static const char		prvname[] = "syscall";
static const char		modname[] = "vmlinux";

#define SYSCALLSFS		EVENTSFS "syscalls/"

/*
 * We need to skip over an extra field: __syscall_nr.
 */
#define SKIP_EXTRA_FIELDS	1

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

struct syscall_data {
	dt_pt_regs	*regs;
	long		syscall_nr;
	long		arg[6];
};

#define SCD_ARG(n)	offsetof(struct syscall_data, arg[n])

#define PROBE_LIST	TRACEFS "available_events"

#define PROV_PREFIX	"syscalls:"
#define ENTRY_PREFIX	"sys_enter_"
#define EXIT_PREFIX	"sys_exit_"

/* Scan the PROBE_LIST file and add probes for any syscalls events. */
static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	FILE		*f;
	char		buf[256];
	int		n = 0;

	prv = dt_provider_create(dtp, prvname, &dt_syscall, &pattr);
	if (prv == NULL)
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		char	*p;

		/* Here buf is "group:event".  */
		p = strchr(buf, '\n');
		if (p)
			*p = '\0';
		else {
			/*
			 * If we didn't see a newline, the line was too long.
			 * Report it, and skip until the end of the line.
			 */
			fprintf(stderr, "%s: Line too long: %s\n",
				PROBE_LIST, buf);
			do
				fgets(buf, sizeof(buf), f);
			while (strchr(buf, '\n') == NULL);
			continue;
		}

		/* We need "group:" to match "syscalls:". */
		p = buf;
		if (memcmp(p, PROV_PREFIX, sizeof(PROV_PREFIX) - 1) != 0)
			continue;

		p += sizeof(PROV_PREFIX) - 1;
		/*
		 * Now p will be just "event", and we are only interested in
		 * events that match "sys_enter_*" or "sys_exit_*".
		 */
		if (!memcmp(p, ENTRY_PREFIX, sizeof(ENTRY_PREFIX) - 1)) {
			p += sizeof(ENTRY_PREFIX) - 1;
			if (dt_probe_insert(dtp, prv, prvname, modname, p,
					    "entry"))
				n++;
		} else if (!memcmp(p, EXIT_PREFIX, sizeof(EXIT_PREFIX) - 1)) {
			p += sizeof(EXIT_PREFIX) - 1;
			if (dt_probe_insert(dtp, prv, prvname, modname, p,
					    "return"))
				n++;
		}
	}

	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a syscall probe.
 *
 * The trampoline function is called when a syscall probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_syscall(struct syscall_data *scd)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static void trampoline(dt_pcb_t *pcb)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;
	uint_t		lbl_exit = dt_irlist_label(dlp);
	dt_ident_t	*idp;

#define DCTX_FP(off)	(-(ushort_t)DCTX_SIZE + (ushort_t)(off))

	/*
	 * int dt_syscall(struct syscall_data *scd)
	 * {
	 *     dt_dctx_t	dctx;
	 *
	 *     memset(&dctx, 0, sizeof(dctx));
	 *
	 *     dctx.epid = EPID;
	 *     (we clear dctx.pad and dctx.fault because of the memset above)
	 */
	idp = dt_dlib_get_var(pcb->pcb_hdl, "EPID");
	assert(idp != NULL);
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_EPID), -1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_PAD), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_FAULT), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

#if 0
	/*
	 *     (we clear the dctx.regs space because of the memset above)
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP,
				      DCTX_FP(DCTX_REGS) + i, 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
#endif

	/*
	 *     for (i = 0; i < argc; i++)
	 *	   dctx.argv[i] = scd->arg[i];
	 */
	for (i = 0; i < pcb->pcb_pinfo.dtp_argc; i++) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, SCD_ARG(i));
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(i)),
				  BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *     (we clear dctx.argv[6] and on because of the memset above)
	 */
	for (i = pcb->pcb_pinfo.dtp_argc;
	     i < ARRAY_SIZE(((dt_dctx_t *)0)->argv); i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(i)),
				      0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 * We know the BPF context (scd) is in %r1.  Since we will be passing
	 * the DTrace context (dctx) as 2nd argument to dt_predicate() (if
	 * there is a predicate) and dt_program, we need it in %r2.
	 */
	instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(0));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     rc = dt_program(scd, dctx);
	 */
	idp = dt_dlib_get_func(pcb->pcb_hdl, "dt_program");
	assert(idp != NULL);
	instr = BPF_CALL_FUNC(idp->di_id);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;

	/*
	 * exit:
	 *     return rc;
	 * }
	 */
	instr = BPF_RETURN();
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_exit, instr));
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *idp, int *argcp, dt_argdesc_t **argvp)
{
	FILE	*f;
	char	fn[256];
	int	rc;

	*idp = -1;

	/*
	 * We know that the probe name is either "entry" or "return", so we can
	 * just check the first character.
	 */
	strcpy(fn, SYSCALLSFS);
	if (prp->desc->prb[0] == 'e')
		strcat(fn, "sys_enter_");
	else
		strcat(fn, "sys_exit_");
	strcat(fn, prp->desc->fun);
	strcat(fn, "/format");

	f = fopen(fn, "r");
	if (!f)
		return -ENOENT;

	rc = tp_event_info(dtp, f, SKIP_EXTRA_FIELDS, idp, argcp, argvp);
	fclose(f);

	return rc;
}

dt_provimpl_t	dt_syscall = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_TRACEPOINT,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.probe_info	= &probe_info,
};
