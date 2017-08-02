/*
 * Oracle Linux DTrace.
 * Copyright © 2010, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on library net.d
#pragma D depends_on provider ip
#pragma D depends_on provider tcp

inline int TH_FIN =	0x01;
inline int TH_SYN =	0x02;
inline int TH_RST =	0x04;
inline int TH_PSH =	0x08;
inline int TH_ACK =	0x10;
inline int TH_URG =	0x20;
inline int TH_ECE =	0x40;
inline int TH_CWR =	0x80;

inline int TCP_STATE_IDLE = 		0x00;
inline int TCP_STATE_ESTABLISHED =	0x01;
inline int TCP_STATE_SYN_SENT =		0x02;
inline int TCP_STATE_SYN_RECEIVED =	0x03;
inline int TCP_STATE_FIN_WAIT_1 =	0x04;
inline int TCP_STATE_FIN_WAIT_2 =	0x05;
inline int TCP_STATE_TIME_WAIT =	0x06;
inline int TCP_STATE_CLOSED =		0x07;
inline int TCP_STATE_CLOSE_WAIT =	0x08;
inline int TCP_STATE_LAST_ACK =		0x09;
inline int TCP_STATE_LISTEN =		0x0a;
inline int TCP_STATE_CLOSING =		0x0b;

/*
 * Convert a TCP state value to a string.
 */
inline string tcp_state_string[int state] =
	state == TCP_STATE_CLOSED ? "state-closed" :
	state == TCP_STATE_IDLE ? "state-idle" :
	state == TCP_STATE_LISTEN ? "state-listen" :
	state == TCP_STATE_SYN_SENT ? "state-syn-sent" :
	state == TCP_STATE_SYN_RECEIVED ? "state-syn-received" :
	state == TCP_STATE_ESTABLISHED ? "state-established" :
	state == TCP_STATE_CLOSE_WAIT ? "state-close-wait" :
	state == TCP_STATE_FIN_WAIT_1 ? "state-fin-wait-1" :
	state == TCP_STATE_CLOSING ? "state-closing" :
	state == TCP_STATE_LAST_ACK ? "state-last-ack" :
	state == TCP_STATE_FIN_WAIT_2 ? "state-fin-wait-2" :
	state == TCP_STATE_TIME_WAIT ? "state-time-wait" :
	"<unknown>";
#pragma D binding "1.6.3" tcp_state_string

/*
 * tcpinfo is the TCP header fields.
 */
typedef struct tcpinfo {
	uint16_t tcp_sport;		/* source port */
	uint16_t tcp_dport;		/* destination port */
	uint32_t tcp_seq;		/* sequence number */
	uint32_t tcp_ack;		/* acknowledgment number */
	uint8_t tcp_offset;		/* data offset, in bytes */
	uint8_t tcp_flags;		/* flags */
	uint16_t tcp_window;		/* window size */
	uint16_t tcp_checksum;		/* checksum */
	uint16_t tcp_urgent;		/* urgent data pointer */
	uintptr_t tcp_hdr;		/* raw TCP header */
} tcpinfo_t;

/*
 * tcpsinfo contains stable TCP details from tcp_t.
 */
typedef struct tcpsinfo {
	uintptr_t tcps_addr;
	int tcps_local;			/* is delivered locally, boolean */
	uint16_t tcps_lport;		/* local port */
	uint16_t tcps_rport;		/* remote port */
	string tcps_laddr;		/* local address, as a string */
	string tcps_raddr;		/* remote address, as a string */
	int tcps_state;			/* TCP state */
	uint32_t tcps_iss;		/* Initial sequence # sent */
	uint32_t tcps_suna;		/* sequence # sent but unacked */
	uint32_t tcps_snxt;		/* next sequence # to send */
	uint32_t tcps_rnxt;		/* next sequence # expected */
	uint32_t tcps_swnd;		/* send window size */
	int32_t tcps_snd_ws;		/* send window scaling */
	uint32_t tcps_rwnd;		/* receive window size */
	int32_t tcps_rcv_ws;		/* receive window scaling */
	uint32_t tcps_cwnd;		/* congestion window */
	uint32_t tcps_cwnd_ssthresh;	/* threshold for congestion avoidance */
	uint32_t tcps_sack_snxt;	/* next SACK seq # for retransmission */
	uint32_t tcps_rto;		/* round-trip timeout, msec */
	uint32_t tcps_mss;		/* max segment size */
	int tcps_retransmit;		/* retransmit send event, boolean */
	uint32_t tcps_rtt;		/* smoothed avg round-trip time, msec */
	uint32_t tcps_rtt_sd;		/* standard deviation of RTT */
	uint32_t tcps_irs;		/* Initial recv sequence # */
} tcpsinfo_t;

/*
 * tcplsinfo provides the old tcp state for state changes.
 */
typedef struct tcplsinfo {
	int tcps_state;		/* previous TCP state */
} tcplsinfo_t;

#pragma D binding "1.6.3" translator
translator tcpinfo_t < struct tcphdr *T > {
	tcp_sport = T ? ntohs(T->source) : 0;
	tcp_dport = T ? ntohs(T->dest) : 0;
	tcp_seq = T ? ntohl(T->seq) : 0;
	tcp_ack = T ? ntohl(T->ack_seq) : 0;
	tcp_offset = T ? (*(uint8_t *)(T + 12) & 0xf0) >> 2 : 0;
	tcp_flags = T ? *(uint8_t *)(T + 13) : 0;
	tcp_window = T ? ntohs(T->window) : 0;
	tcp_checksum = T ? ntohs(T->check) : 0;
	tcp_urgent = T ? ntohs(T->urg_ptr) : 0;
	tcp_hdr = (uintptr_t)T;
};

/*
 * In the main we simply translate from the "struct [tcp_]sock *" to
 * a tcpsinfo_t *.  However there are a few exceptions:
 *
 * - tcps_state is always derived from arg6.  The reason is that in some
 * state transitions sock->sk_state does not reflect the actual TCP
 * connection state.  For example the TIME_WAIT state is handled in
 * Linux by creating a separate timewait socket and the state of the
 * original socket is CLOSED.  In some other cases we also need to
 * instrument state transition prior to the update of sk_state.  To do
 * all of this we rely on arg6.
 * - we sometimes need to retrieve local/remote port/address settings from
 * TCP and IP headers directly, for example prior to the address/port
 * being committed to the socket.  To do this effectively we need to know
 * if the packet data is inbound (in which case the local IP/port are the
 * destination) or outbound (in which case the local IP/port are the source).
 * arg7 is set to 0 for outbound traffic and 1 for inbound so we use these
 * to reconstruct the address/port info where necessary.  arg2 used for IP
 * information while arg4 contains the TCP header, so it is used for port data.
 * NET_PROBE_INBOUND is defined as 1, NET_PROBE_OUTBOUND as 0 in net.d.
 */
#pragma D binding "1.6.3" translator
translator tcpsinfo_t < struct tcp_sock *T > {
	tcps_addr = (uintptr_t)T;
	tcps_local =
	    T && ((struct sock *)T)->__sk_common.skc_family == AF_INET ?
	    (((struct sock *)T)->__sk_common.skc_rcv_saddr ==
	    ((struct sock *)T)->__sk_common.skc_daddr) :
	    T && ((struct sock *)T)->__sk_common.skc_family == AF_INET6 ?
	    (((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_rcv_saddr)[0]
	    ==
	    ((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_daddr)[0] &&
	    ((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_rcv_saddr)[1]
            ==
            ((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_daddr)[1] &&
	    ((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_rcv_saddr)[2]
            ==
            ((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_daddr)[2] &&
	    ((uint32_t *)&((struct sock *)T)->__sk_common.skc_v6_rcv_saddr)[3])
	    : 0;
	tcps_lport = (T && ((struct inet_sock *)T)->inet_sport != 0) ?
	    ntohs(((struct inet_sock *)T)->inet_sport) :
	    (T && ((struct inet_sock *)T)->inet_sport == 0) ?
	    ntohs(((struct sock *)T)->__sk_common.skc_num) :
	    arg4 != NULL ?
	    ntohs(arg7 == NET_PROBE_INBOUND ?
	    ((struct tcphdr *)arg4)->dest : ((struct tcphdr *)arg4)->source) :
	    0;
	tcps_rport = T && ((struct sock *)T)->__sk_common.skc_dport != 0 ?
	    ntohs(((struct sock *)T)->__sk_common.skc_dport) :
	    arg4 != NULL ?
	    ntohs(arg7 == NET_PROBE_INBOUND ?
            ((struct tcphdr *)arg4)->source : ((struct tcphdr *)arg4)->dest) :
	    0;
	tcps_laddr =
	    T && ((struct sock *)T)->__sk_common.skc_family == AF_INET ?
	    inet_ntoa(&((struct sock *)T)->__sk_common.skc_rcv_saddr) :
	    T && ((struct sock *)T)->__sk_common.skc_family == AF_INET6 ?
	    inet_ntoa6(&((struct sock *)T)->__sk_common.skc_v6_rcv_saddr) :
	    arg2 != NULL && (*(uint8_t *)arg2) >> 4 == 4 ?
	    inet_ntoa(arg7 == NET_PROBE_INBOUND ?
	    &((struct iphdr *)arg2)->daddr : &((struct iphdr *)arg2)->saddr) :
	    arg2 != NULL && *((uint8_t *)arg2) >> 4 == 6 ?
	    inet_ntoa6(arg7 == NET_PROBE_INBOUND ?
	    &((struct ipv6hdr *)arg2)->daddr :
	    &((struct ipv6hdr *)arg2)->saddr) :
	    "<unknown>";
	tcps_raddr =
	    T && ((struct sock *)T)->__sk_common.skc_family == AF_INET ?
	    inet_ntoa(&((struct sock *)T)->__sk_common.skc_daddr) :
	    T && ((struct sock *)T)->__sk_common.skc_family == AF_INET6 ?
	    inet_ntoa6(&((struct sock *)T)->__sk_common.skc_v6_daddr) :
	    arg2 != NULL && (*(uint8_t *)arg2) >> 4 == 4 ?
	    inet_ntoa(arg7 == NET_PROBE_INBOUND ?
	    &((struct iphdr *)arg2)->saddr : &((struct iphdr *)arg2)->daddr) :
	    arg2 != NULL && *((uint8_t *)arg2) >> 4 == 6 ?
	    inet_ntoa6(arg7 == NET_PROBE_INBOUND ?
	    &((struct ipv6hdr *)arg2)->saddr :
	    &((struct ipv6hdr *)arg2)->daddr) :
	    "<unknown>";
	tcps_state = arg6;
	tcps_iss = T ?
	    T->snd_una - (uint32_t)T->bytes_acked : 0;
	tcps_suna = T ? T->snd_una : 0;
	tcps_snxt = T ? T->snd_nxt : 0;
	tcps_rnxt = T ? T->rcv_nxt : 0;
	tcps_swnd = T ? T->snd_wnd : 0;
	tcps_snd_ws = T ? T->rx_opt.snd_wscale : 0;
	tcps_rwnd = T ? T->rcv_wnd : 0;
	tcps_rcv_ws = T ? T->rx_opt.rcv_wscale : 0;
	tcps_cwnd = T ? T->snd_cwnd : 0;
	tcps_cwnd_ssthresh = T ? T->snd_ssthresh : 0;
	tcps_sack_snxt = (T && T->sacked_out == 0) ? T->snd_una :
	    (T && T->highest_sack == NULL) ? T->snd_nxt :
	    (T && T->highest_sack != NULL) ?
	    ((struct tcp_skb_cb *)&((T->highest_sack->cb[0])))->seq : 0;
	tcps_rto = T ? T->inet_conn.icsk_rto : 0;
	tcps_mss = T ? T->mss_cache : 0;
	tcps_retransmit = T && arg0 ?
	    (((struct tcp_skb_cb *)&(((struct sk_buff *)arg0)->cb[0]))->end_seq
	    < T->snd_nxt - 1) : 0;
	tcps_rtt = T ? (T->srtt_us >> 3)/1000 : 0;
	tcps_rtt_sd = T ? (T->mdev_us >> 2)/1000 : 0;
	tcps_irs = T && T->bytes_received > 0 ?
	    T->rcv_nxt - (uint32_t)T->bytes_received : 0;
};

#pragma D binding "1.6.3" translator
translator tcplsinfo_t < int I > {
	tcps_state = I;
};
