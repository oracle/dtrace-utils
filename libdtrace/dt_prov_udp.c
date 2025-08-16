/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'udp' SDT provider for DTrace-specific probes.
 */
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

static const char		prvname[] = "udp";
static const char		modname[] = "vmlinux";

enum {
	NET_PROBE_OUTBOUND = 0,
	NET_PROBE_INBOUND,
};

static probe_dep_t	probes[] = {
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::udp_queue_rcv_skb:entry" },
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::udpv6_queue_rcv_skb:entry" },
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::ip_send_skb:entry" },
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::ip6_send_skb:entry" },
	{ NULL, }
};

static probe_arg_t probe_args[] = {

	{ "receive", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "receive", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "receive", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "receive", 3, { 3, 0, "struct udp_sock *", "udpsinfo_t *" } },
	{ "receive", 4, { 4, 0, "struct udphdr *", "udpinfo_t *" } },

	{ "send", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "send", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "send", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "send", 3, { 3, 0, "struct udp_sock *", "udpsinfo_t *" } },
	{ "send", 4, { 4, 0, "struct udphdr *", "udpinfo_t *" } },

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
 * Provide all the "udp" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	return dt_sdt_populate(dtp, prvname, modname, &dt_udp, &pattr,
			       probe_args, probes);
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_udp(void *data)
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
	int		skbarg = 1;
	int		direction;

	/*
	 * We construct the udp::: probe arguments as follows:
	 *      arg0 = skb
	 *      arg1 = sk
	 *      arg2 = ip_hdr(skb) [if available]
	 *      arg3 = sk [struct udp_sock *]
	 *      arg4 = udp_hdr(skb)
	 *      arg5 = NET_PROBE_INBOUND (0x1) | NET_PROBE_OUTBOUND (0x0)
	 * arg5 never makes it into supported args[], it is simply set to
	 * help inform translators about whether it is an inbound/outbound probe
	 */

	if (strcmp(prp->desc->prb, "receive") == 0) {
		direction = NET_PROBE_INBOUND;
		/* get sk from arg0, store in arg3 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(0)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_6, 0, exitlbl));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_6));
	} else {
		if (strcmp(uprp->desc->fun, "ip6_send_skb") == 0)
			skbarg = 0;
		direction = NET_PROBE_OUTBOUND;
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(skbarg)));
		/* get sk from skb->sk, store in arg3 */
		dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6, "sk");
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, exitlbl));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_0));
	}

	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(skbarg)));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_6));

	/* Now get sk from arg3, store it in arg1 and ensure it is UDP */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(3)));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));
	dt_cg_tramp_get_member(pcb, "struct sock", BPF_REG_6,
			       "sk_protocol");
	emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, IPPROTO_UDP, exitlbl));

	/*
	 * ip_hdr(skb) =
	 *	skb_network_header(skb)	=	(include/linux/ip.h)
	 *	skb->head + skb->network_header	(include/linux/skbuff.h)
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(0)));
	dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6, "head");
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));

	dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6,
			       "network_header");
	emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));

	/*
	 * udp_hdr(skb) =
	 *	skb_transport_header(skb) =		(include/linux/ip.h)
	 *	skb->head + skb->transport_header	(include/linux/skbuff.h)
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(0)));
	dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6, "head");
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
	dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6,
			       "transport_header");
	emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));

	emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(5), direction));

	return 0;
}

dt_provimpl_t	dt_udp = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
