/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The 'tcp' SDT provider for DTrace-specific probes.
 */
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_sdt.h"
#include "dt_probe.h"

static const char		prvname[] = "tcp";
static const char		modname[] = "vmlinux";

enum {
	NET_PROBE_OUTBOUND = 0,
	NET_PROBE_INBOUND,
	NET_PROBE_STATE
};

static probe_dep_t	probes[] = {
	/* does not fire on UEK7 unless rawfbt; no idea why... */
	{ "accept-established",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::tcp_init_transfer:entry" },
	{ "accept-refused",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_v4_send_reset:entry" },
	{ "accept-refused",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_v6_send_reset:entry" },
	{ "connect-established",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_finish_connect:entry" },
	{ "connect-refused",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_reset:entry" },
	{ "connect-request",
	/* ip_queue_xmit() is static for older kernels so use __ip_queue_xmit()
	 * which is non-static for older and newer kernels.
	 */
	  DTRACE_PROBESPEC_NAME,	"fbt::__ip_queue_xmit:entry" },
	/* ip6_xmit has > 6 args so cannot fentry on aarch64; use rawfbt */
	{ "connect-request",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::ip6_xmit:entry" },
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_rcv_established:entry" },
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_rcv_state_process:entry" },
	{ "receive",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_v4_send_reset:entry" },
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"fbt::__ip_queue_xmit:entry" },
	/* ip_send_unicast_reply has 10 args so cannot fentry; use rawfbt */
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::ip_send_unicast_reply:entry" },
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"fbt::ip_build_and_send_pkt" },
	/* ip6_xmit has > 6 args so cannot fentry on aarch64; use rawfbt */
	{ "send",
	  DTRACE_PROBESPEC_NAME,	"rawfbt::ip6_xmit:entry" },
	{ "state-change",
	  DTRACE_PROBESPEC_NAME,	"sdt:::inet_sock_set_state" },
	{ "state-change",
	  DTRACE_PROBESPEC_NAME,	"fbt::tcp_time_wait:entry" },
	{ "state-change",
	  DTRACE_PROBESPEC_NAME,	"fbt::inet_csk_clone_lock:entry" },
	{ NULL, }
};

static probe_arg_t probe_args[] = {
	{ "accept-established", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "accept-established", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "accept-established", 2, { 2, 0, "__dtrace_tcp_void_ip_t *", "ipinfo_t *" } },
	{ "accept-established", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "accept-established", 4, { 4, 0, "__dtrace_tcp_void_tcp_t *", "tcpinfo_t *" } },
	{ "accept-established", 5, { 5, 0, "void", "void" } },

	{ "accept-refused", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "accept-refused", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "accept-refused", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "accept-refused", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "accept-refused", 4, { 4, 0, "struct tcphdr *", "tcpinfo_t *" } },
	{ "accept-refused", 5, { 5, 0, "void", "void"} },

	{ "connect-established", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "connect-established", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "connect-established", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "connect-established", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "connect-established", 4, { 4, 0, "struct tcphdr *", "tcpinfo_t *" } },
	{ "connect-established", 5, { 5, 0, "void", "void"} },

	{ "connect-refused", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "connect-refused", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "connect-refused", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "connect-refused", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "connect-refused", 4, { 4, 0, "struct tcphdr *", "tcpinfo_t *" } },
	{ "connect-refused", 5, { 5, 0, "void", "void"} },

	{ "connect-request", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "connect-request", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "connect-request", 2, { 2, 0, "__dtrace_tcp_void_ip_t *", "ipinfo_t *" } },
	{ "connect-request", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "connect-request", 4, { 4, 0, "struct tcphdr *", "tcpinfo_t *" } },
	{ "connect-request", 5, { 5, 0, "void", "void"} },

	{ "receive", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "receive", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "receive", 2, { 2, 0, "void_ip_t *", "ipinfo_t *" } },
	{ "receive", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "receive", 4, { 4, 0, "struct tcphdr *", "tcpinfo_t *" } },
	{ "receive", 5, { 5, 0, "void", "void"} },

	{ "send", 0, { 0, 0, "struct sk_buff *", "pktinfo_t *" } },
	{ "send", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "send", 2, { 2, 0, "__dtrace_tcp_void_ip_t *", "ipinfo_t *" } },
	{ "send", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "send", 4, { 4, 0, "struct tcphdr *", "tcpinfo_t *" } },
	{ "send", 5, { 5, 0, "void", "void"} },

	{ "state-change", 0, { 0, 0, "void", "void", } },
	{ "state-change", 1, { 1, 0, "struct sock *", "csinfo_t *" } },
	{ "state-change", 2, { 2, 0, "void", "void" } },
	{ "state-change", 3, { 3, 0, "struct tcp_sock *", "tcpsinfo_t *" } },
	{ "state-change", 4, { 4, 0, "void", "void" } },
	{ "state-change", 5, { 5, 0, "int", "tcplsinfo_t *" } },

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
 * Provide all the "tcp" SDT probes.
 */
static int populate(dtrace_hdl_t *dtp)
{
	return dt_sdt_populate(dtp, prvname, modname, &dt_tcp, &pattr,
			       probe_args, probes);
}

/*
 * Generate a BPF trampoline for a SDT probe.
 *
 * The trampoline function is called when a SDT probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_tcp(void *data)
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
	int		direction, have_iphdr;
	int		skarg = 0, skbarg = 1, tcparg = 0;
	int		skarg_maybe_null = 0, have_skb = 1;
	int		skstate = 0;
#ifdef HAVE_LIBCTF
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dtrace_typeinfo_t sym;
	ctf_funcinfo_t	fi;
	int		rc;
#endif

	/*
	 * We construct the tcp::: probe arguments as follows:
	 *      arg0 = skb
	 *      arg1 = sk
	 *      arg2 = ip_hdr(skb) [if available]
	 *      arg3 = sk [struct tcp_sock *]
	 *      arg4 = tcp_hdr(skb)
	 *      arg5 = new_sk_state [for state_change]
	 *      arg6 = NET_PROBE_INBOUND (0x1) | NET_PROBE_OUTBOUND (0x0) |
	 *      	  NET_PROBE_STATE (0x2)
	 * arg6 never makes it into supported args[], it is simply set to
	 * help inform translators about whether it is an inbound, outbound or
	 * state transition probe.
	 */

	if (strcmp(prp->desc->prb, "state-change") == 0) {
		int newstatearg;
		int skip_state = 0;
		int check_proto = IPPROTO_TCP;

		/* For pre-6.14 kernels, inet_sock_state_change() to
		 * TCP_SYN_RCV is broken in that the cloned socket has
		 * not yet copied info of interest like addresses, ports.
		 * This is fixed in 6.14 via
		 *
		 * commit a3a128f611a965fddf8a02dd45716f96e0738e00
		 * Author: Eric Dumazet <edumazet@google.com>
		 * Date:   Wed Feb 12 13:13:28 2025 +0000
		 * 
		 * inet: consolidate inet_csk_clone_lock()
		 *
		 * To work around this we trace inet_csk_clone_lock and
		 * use the reqsk (arg1) as the means to populate the
		 * struct tcpinfo.  We need then to explictly set the
		 * state to TCP_SYN_RCV and also skip the case where
		 * inet_sock_set_state() specifies TCP_SYN_RCV otherwise
		 * we will get a probe double-firing.  So we set skip_state
		 * to that state to avoid that double-firing.
		 */
		if (strcmp(uprp->desc->fun, "inet_csk_clone_lock") == 0) {
			skarg = 1;
			newstatearg = 2;
			check_proto = 0;
			emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(2),
						BPF_TCP_SYN_RECV));
		} else if (strcmp(uprp->desc->fun, "tcp_time_wait") == 0) {
			skarg = 0;
			newstatearg = 1;
		} else {
			skarg = 0;
			newstatearg = 2;
			skip_state = BPF_TCP_SYN_RECV;
		}
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(skarg)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_6, 0, exitlbl));
		/* check it is a TCP socket */
		if (check_proto) {
			dt_cg_tramp_get_member(pcb, "struct sock", BPF_REG_6,
					 "sk_protocol");
			emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0,
						 IPPROTO_TCP, exitlbl));
		}
		/* save sk */
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_6));

		/* save new state */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(newstatearg)));
		if (skip_state) {
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_6, skip_state,
						 exitlbl));
		}
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_6));

		/* save sk */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(3)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_6));

		/* save empty args */
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(0), 0));
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(2), 0));
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(4), 0));

		/* NET_PROBE_STATE */
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(6),
					NET_PROBE_STATE));
		return 0;
	}

	if (strcmp(prp->desc->prb, "accept-established") == 0) {
		direction = NET_PROBE_INBOUND;
#ifdef HAVE_LIBCTF
		have_iphdr = 1;
		/* on older (5.4) kernels, tcp_init_transfer() only has 2
		 * args, i.e. no struct skb * third argument.
 		 */
		rc = dtrace_lookup_by_type(dtp, DTRACE_OBJ_EVERY,
					   uprp->desc->fun, &sym);
		if (rc == 0 &&
		    ctf_type_kind(sym.dtt_ctfp, sym.dtt_type) == CTF_K_FUNCTION &&
		    ctf_func_type_info(sym.dtt_ctfp, sym.dtt_type, &fi) == 0 &&
		    fi.ctc_argc > 2) {
			/* skb in arg2 not arg1 */
			skbarg = 2;
		} else {
			have_skb = 0;
			have_iphdr = 0;
		}
#else
		have_skb = 0;
		have_iphdr = 0;
#endif
		/* ensure arg1 is BPF_SOCK_OPS_PASSIVE_ESTABLISHED_CB */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(1)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_6,
					 BPF_SOCK_OPS_PASSIVE_ESTABLISHED_CB,
					 exitlbl));
	} else if (strcmp(prp->desc->prb, "receive") == 0 ||
		   strcmp(prp->desc->prb, "accept-refused") == 0) {
		direction = NET_PROBE_INBOUND;
		have_iphdr = 1;
		if (strcmp(uprp->desc->fun, "tcp_v4_send_reset") == 0 ||
		    strcmp(uprp->desc->fun, "tcp_v6_send_reset") == 0)
			skarg_maybe_null = 1;
	} else if (strcmp(prp->desc->prb, "connect-established") == 0) {
		direction = NET_PROBE_INBOUND;
		have_iphdr = 1;
	} else if (strcmp(prp->desc->prb, "connect-refused") == 0) {
		direction = NET_PROBE_INBOUND;
		have_iphdr = 1;
		skstate = BPF_TCP_SYN_SENT;
	} else {
		direction = NET_PROBE_OUTBOUND;
		if (strcmp(uprp->desc->fun, "ip_send_unicast_reply") == 0) {
#ifdef HAVE_LIBCTF
			/* Newer kernels pass the original socket as second
			 * arg to ip_send_unicast_reply(); if that function
			 * has an extra (> 9) argument we know we have to
			 * find sk, skb in arg1, arg2 not arg0, arg1.
			 * tcp header is in ip_reply_arg which is in
			 * arg5/arg6 depending on whether extra parameter
			 * for original sk is present.
			 */
			rc = dtrace_lookup_by_type(dtp, DTRACE_OBJ_EVERY,
						   uprp->desc->fun, &sym);
			if (rc == 0 &&
			    ctf_type_kind(sym.dtt_ctfp, sym.dtt_type) == CTF_K_FUNCTION &&
			    ctf_func_type_info(sym.dtt_ctfp, sym.dtt_type, &fi) == 0 &&
			    fi.ctc_argc > 9) {
				/* NULL sk in arg1 not arg2 (dont want ctl_sk) */
				skarg = 1;
				/* skb in arg2 not arg1 */
				skbarg = 2;
				tcparg = 6;
			} else {
				skarg = 0;
				skbarg = 1;
				tcparg = 5;
			}
#else
			skarg = 0;
			skbarg = 1;
			tcparg = 5;
#endif
			have_iphdr = 1;
			tcparg = 6;
			skarg_maybe_null = 1;
		} else if (strcmp(uprp->desc->fun, "ip_build_and_send_pkt") == 0) {
			skarg = 1;
			skbarg = 0;
			have_iphdr = 0;
			skarg_maybe_null = 1;
		} else if (strcmp(prp->desc->prb, "connect-request") == 0) {
			skstate = BPF_TCP_SYN_SENT;
			have_iphdr = 0;
		} else
			have_iphdr = 0;
	}

	/* first save sk to args[3]; this avoids overwriting it when we
	 * populate args[0,1] below.
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(skarg)));
	/* only allow NULL sk for ip_send_unicast_reply() */
	if (!skarg_maybe_null)
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_6, 0, exitlbl));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_6));

	if (have_skb) {
		/* then save skb to args[0] */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(skbarg)));
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_6, 0, exitlbl));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_6));
	} else {
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(0), 0));
	}

	/* next save sk to args[1] now that we have skb in args[0] */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(3)));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_6));

	/*
	 * ip_hdr(skb) =
	 *	skb_network_header(skb)	=	(include/linux/ip.h)
	 *	skb->head + skb->network_header	(include/linux/skbuff.h)
	 */
	if (have_skb && have_iphdr) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(0)));
		dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6, "head");
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
		dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6,
				       "network_header");
		emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
	} else {
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(2), 0));
	}

	/*
	 * tcp_hdr(skb) =
	 *	skb_transport_header(skb) =		(include/linux/ip.h)
	 *	skb->head + skb->transport_header	(include/linux/skbuff.h)
	 */
	if (have_skb) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(tcparg)));
		if (tcparg) {
			/* struct ip_reply_arg * has a kvec containing the tcp header */
			dt_cg_tramp_get_member(pcb, "struct kvec", BPF_REG_6, "iov_base");
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
		} else {
			dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6, "head");
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
			dt_cg_tramp_get_member(pcb, "struct sk_buff", BPF_REG_6,
					 "transport_header");
			emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
		}
	} else {
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(4), 0));
	}

	if (!skarg_maybe_null) {
		/* save sk state */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_6, BPF_REG_7, DMST_ARG(3)));
		dt_cg_tramp_get_member(pcb, "struct sock_common", BPF_REG_6,
				 "skc_state");
		/* ensure sk state - if specified - is what we expect */
		if (skstate)
			emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, skstate,
						 exitlbl));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0));
	}
	emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(6), direction));

	return 0;
}

dt_provimpl_t	dt_tcp = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.populate	= &populate,
	.enable		= &dt_sdt_enable,
	.load_prog	= &dt_bpf_prog_load,
	.trampoline	= &trampoline,
	.probe_info	= &dt_sdt_probe_info,
	.destroy	= &dt_sdt_destroy,
};
