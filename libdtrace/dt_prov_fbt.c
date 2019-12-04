/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The Function Boundary Tracing (FBT) provider for DTrace.
 *
 * FBT probes are exposed by the kernel as kprobes.  They are listed in the
 * TRACEFS/available_filter_functions file.  Some kprobes are associated with
 * a specific kernel module, while most are in the core kernel.
 *
 * Mapping from event name to DTrace probe name:
 *
 *	<name>					fbt:vmlinux:<name>:entry
 *						fbt:vmlinux:<name>:return
 *   or
 *	<name> [<modname>]			fbt:<modname>:<name>:entry
 *						fbt:<modname>:<name>:return
 *
 * Mapping from BPF section name to DTrace probe name:
 *
 *	kprobe/<name>				fbt:vmlinux:<name>:entry
 *	kretprobe/<name>			fbt:vmlinux:<name>:return
 *
 * (Note that the BPF section does not carry information about the module that
 *  the function is found in.  This means that BPF section name cannot be used
 *  to distinguish between functions with the same name occurring in different
 *  modules.)
 */
#include <assert.h>
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

#define KPROBE_EVENTS		TRACEFS "kprobe_events"
#define PROBE_LIST		TRACEFS "available_filter_functions"

static const char		provname[] = "fbt";
static const char		modname[] = "vmlinux";

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

/*
 * Scan the PROBE_LIST file and add entry and return probes for every function
 * that is listed.
 */
static int fbt_populate(dtrace_hdl_t *dtp)
{
	dt_provider_t		*prv;
	FILE			*f;
	char			buf[256];
	char			*p;
	const char		*mod = modname;
	int			n = 0;
	dtrace_syminfo_t	sip;
	dtrace_probedesc_t	pd;

	if (!(prv = dt_provider_create(dtp, "fbt", &dt_fbt, &pattr)))
		return 0;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {
		/*
		 * Here buf is either "funcname\n" or "funcname [modname]\n".
		 */
		p = strchr(buf, '\n');
		if (p) {
			*p = '\0';
			if (p > buf && *(--p) == ']')
				*p = '\0';
		} else {
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

		/*
		 * Now buf is either "funcname" or "funcname [modname".  If
		 * there is no module name provided, we will use the default.
		 */
		p = strchr(buf, ' ');
		if (p) {
			*p++ = '\0';
			if (*p == '[')
				p++;
		}

		/*
		 * If we did not see a module name, perform a symbol lookup to
		 * try to determine the module name.
		 */
		if (!p) {
			if (dtrace_lookup_by_name(dtp, DTRACE_OBJ_KMODS, buf,
						  NULL, &sip) == 0)
				mod = sip.object;
		} else
			mod = p;

		/*
		 * Due to the lack of module names in
		 * TRACEFS/available_filter_functions, there are some duplicate
		 * function names.  We need to make sure that we do not create
		 * duplicate probes for these.
		 */
		pd.id = DTRACE_IDNONE;
		pd.prv = provname;
		pd.mod = mod;
		pd.fun = buf;
		pd.prb = "entry";
		if (dt_probe_lookup(dtp, &pd) != NULL)
			continue;

		if (dt_probe_insert(dtp, prv, provname, mod, buf, "entry"))
			n++;
		if (dt_probe_insert(dtp, prv, provname, mod, buf, "return"))
			n++;
	}

	fclose(f);

	return n;
}

/*
 * Generate a BPF trampoline for a FBT probe.
 *
 * The trampoline function is called when a FBT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_fbt(dt_pt_regs *regs)
 *
 * The trampoline will populate a dt_bpf_context struct and then call the
 * function that implements tha compiled D clause.  It returns the value that
 * it gets back from that function.
 */
static void fbt_trampoline(dt_pcb_t *pcb, int haspred)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;
	uint_t		lbl_exit = dt_irlist_label(dlp);
	dt_ident_t	*idp;

#define DCTX_FP(off)	(-(ushort_t)DCTX_SIZE + (ushort_t)(off))

	/*
	 * int dt_fbt(dt_pt_regs *regs)
	 * {
	 *     struct dt_bpf_context	dctx;
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

	/*
	 *     dctx.regs = *regs;
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, i);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_REGS) + i,
				  BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *     dctx.argv[0] = PT_REGS_PARAM1(regs);
	 *     dctx.argv[1] = PT_REGS_PARAM2(regs);
	 *     dctx.argv[2] = PT_REGS_PARAM3(regs);
	 *     dctx.argv[3] = PT_REGS_PARAM4(regs);
	 *     dctx.argv[4] = PT_REGS_PARAM5(regs);
	 *     dctx.argv[5] = PT_REGS_PARAM6(regs);
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(0)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(1)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(2)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG3);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(3)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(4)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG5);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(5)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     (we clear dctx.argv[6] and on because of the memset above)
	 */
	for (i = 6; i < sizeof(((struct dt_bpf_context *)0)->argv) / 8; i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(i)),
				      0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *     if (haspred) {
	 *	   rc = dt_predicate(scd, dctx);
	 *	   if (rc == 0) goto exit;
	 *     }
	 */
	if (haspred) {
		instr = BPF_MOV_REG(BPF_REG_6, BPF_REG_1);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(0));
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		idp = dt_dlib_get_func(pcb->pcb_hdl, "dt_predicate");
		assert(idp != NULL);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		instr = BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *     rc = dt_program(scd, dctx);
	 */
	instr = BPF_MOV_REG(BPF_REG_1, BPF_REG_6);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(0));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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

#if 0
#define ENTRY_PREFIX	"kprobe/"
#define EXIT_PREFIX	"kretprobe/"

/*
 * Perform a probe lookup based on an event name (BPF ELF section name).
 */
static struct dt_probe *fbt_resolve_event(const char *name)
{
	const char	*prbname;
	struct dt_probe	tmpl;
	struct dt_probe	*probe;

	if (!name)
		return NULL;

	if (strncmp(name, ENTRY_PREFIX, sizeof(ENTRY_PREFIX) - 1) == 0) {
		name += sizeof(ENTRY_PREFIX) - 1;
		prbname = "entry";
	} else if (strncmp(name, EXIT_PREFIX, sizeof(EXIT_PREFIX) - 1) == 0) {
		name += sizeof(EXIT_PREFIX) - 1;
		prbname = "return";
	} else
		return NULL;

	memset(&tmpl, 0, sizeof(tmpl));
	tmpl.prv_name = provname;
	tmpl.mod_name = modname;
	tmpl.fun_name = name;
	tmpl.prb_name = prbname;

	probe = dt_probe_by_name(&tmpl);

	return probe;
}

/*
 * Attach the given BPF program (identified by its file descriptor) to the
 * kprobe identified by the given section name.
 *
 * TODO: This should somehow update the probe description with the event ID.
 */
static int fbt_attach(const char *name, int bpf_fd)
{
	char    efn[256];
	char    buf[256];
	int	event_id, fd, rc;

	name += 7;				/* skip "kprobe/" */
	snprintf(buf, sizeof(buf), "p:%s %s\n", name, name);

	/*
	 * Register the kprobe with the tracing subsystem.  This will create
	 * a tracepoint event.
	 */
	fd = open(KPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd < 0) {
		perror(KPROBE_EVENTS);
		return -1;
	}
	rc = write(fd, buf, strlen(buf));
	if (rc < 0) {
		perror(KPROBE_EVENTS);
		close(fd);
		return -1;
	}
	close(fd);

	/* Read the tracepoint event id for the kprobe we just registered. */
	strcpy(efn, EVENTSFS);
	strcat(efn, "kprobes/");
	strcat(efn, name);
	strcat(efn, "/id");

	fd = open(efn, O_RDONLY);
	if (fd < 0) {
		perror(efn);
		return -1;
	}
	rc = read(fd, buf, sizeof(buf));
	if (rc < 0 || rc >= sizeof(buf)) {
		perror(efn);
		close(fd);
		return -1;
	}
	close(fd);
	buf[rc] = '\0';
	event_id = atoi(buf);

	/*
	 * Attaching a BPF program (by file descriptor) to an event (by ID) is
	 * a generic operation provided by the BPF interface code.
	 */
	return dt_bpf_attach(event_id, bpf_fd);

}
#endif

dt_provimpl_t	dt_fbt = {
	.name		= "fbt",
	.populate	= &fbt_populate,
	.trampoline	= &fbt_trampoline,
#if 0
	.resolve_event	= &fbt_resolve_event,
	.attach		= &fbt_attach,
#endif
};
