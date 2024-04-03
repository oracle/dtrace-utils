/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'ip' SDT provider for DTrace-specific probes.
 */
#include <assert.h>
#include <errno.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

static const char		prvname[] = "ip";
static const char		modname[] = "vmlinux";

static probe_dep_t	probes[] = {
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"fbt::ip_local_deliver:entry" },
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"fbt::ip6_input:entry" },
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"fbt::ip_finish_output:entry" },
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"fbt::ip6_finish_output:entry" },
	{ NULL, }
};

static probe_arg_t probe_args[] = {
	{ "receive", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "receive", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "receive", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "receive", 3, { 3, 0, "struct net_device *", "ifinfo_t *" } },
	{ "receive", 4, { 4, 0, "struct iphdr *", "ipv4info_t *" } },
	{ "receive", 5, { 5, 0, "struct ipv6hdr *", "ipv6info_t *"} },
	{ "send", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "send", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "send", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "send", 3, { 3, 0, "struct net_device *", "ifinfo_t *" } },
	{ "send", 4, { 4, 0, "struct iphdr *", "ipv4info_t *" } },
	{ "send", 5, { 5, 0, "struct ipv6hdr *", "ipv6info_t *"} },
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
 * Provide all the "ip" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	return dt_sdt_populate(dtp, prvname, modname, &dt_ip, &pattr,
			       probe_args, probes);
}

/*
 * Retrieve the value of a member in a given struct.
 *
 * Entry:
 *	reg = TYPE *ptr
 *
 * Return:
 *	%r0 = ptr->member
 * Clobbers:
 *	%r1 .. %r5
 */
static int get_member(dt_pcb_t *pcb, const char *name, int reg,
		      const char *member) {
	dtrace_hdl_t		*dtp = pcb->pcb_hdl;
	dt_irlist_t		*dlp = &pcb->pcb_ir;
	dtrace_typeinfo_t	tt;
	ctf_membinfo_t		ctm;
	size_t			size;
	uint_t			ldop;

	if (dtrace_lookup_by_type(dtp, DTRACE_OBJ_KMODS, name, &tt) == -1 ||
	    ctf_member_info(tt.dtt_ctfp, tt.dtt_type, member, &ctm) == CTF_ERR)
		return -1;

	ldop = dt_cg_ldsize(NULL, tt.dtt_ctfp, ctm.ctm_type, &size);

	emit(dlp, BPF_MOV_REG(BPF_REG_3, reg));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, ctm.ctm_offset / NBBY));
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, size));
	emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_TRAMP_SP_BASE));
	emit(dlp, BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));
	emit(dlp, BPF_LOAD(ldop, BPF_REG_0, BPF_REG_FP, DT_TRAMP_SP_BASE));

	return 0;
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_ip(void *data)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns the value that it gets
 * back from that function.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_probe_t	*prp = pcb->pcb_probe;
	dt_probe_t	*uprp = pcb->pcb_parent_probe;
	uint_t		skbreg;

	/*
	 * Determine the register that holds a pointer to the skb passed from
	 * the underlying probe.
	 */
	if (strcmp(prp->desc->prb, "receive") == 0)
		skbreg = 0;
	else
		skbreg = 2;

	/*
	 * We construct the ip:::(receive,send) probe arguments as
	 * follows:
	 *	args[0] = skb
	 *	args[1] = skb->sk
	 *	args[2] = ip_hdr(skb)
	 *	args[3] = skb->dev
	 *	args[4] = [IPv4] ip_hdr(skb)	-or- [IPv6] NULL
	 *	args[5] = [IPv4] NULL		-or- [IPv6] ipv6_hdr(skb)
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(skbreg)));
	emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_6, 0, exitlbl));

	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_6));

	get_member(pcb, "struct sk_buff", BPF_REG_6, "sk");
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));

	/*
	 * ip_hdr(skb) =
	 *	skb_network_header(skb)	=	(include/linux/ip.h)
	 *	skb->head + skb->network_header	(include/linux/skbuff.h)
	 */
	get_member(pcb, "struct sk_buff", BPF_REG_6, "head");
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0));
	get_member(pcb, "struct sk_buff", BPF_REG_6, "network_header");
	emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
	emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
	emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0));

	/*
	 * We can use the name of the underlying probe to determine whether we
	 * are dealing with IPv4 (ip_*) or IPv6 (ip6_*).
	 */
	if (uprp->desc->fun[2] == '6')
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(4), 0));
	else
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(5), 0));

	get_member(pcb, "struct sk_buff", BPF_REG_6, "dev");
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_0));

	return 0;
}

dt_provimpl_t	dt_ip = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
