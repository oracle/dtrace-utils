/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on library net.d
#pragma D depends_on library procfs.d
#pragma D depends_on provider ip

inline int IPPROTO_IP		=	0;
inline int IPPROTO_ICMP		=	1;
inline int IPPROTO_IGMP		=	2;
inline int IPPROTO_IPIP		=	4;
inline int IPPROTO_TCP		=	6;
inline int IPPROTO_EGP		=	8;
inline int IPPROTO_PUP		=	12;
inline int IPPROTO_UDP		=	17;
inline int IPPROTO_IDP		=	22;
inline int IPPROTO_TP		=	29;
inline int IPPROTO_DCCP		=	33;
inline int IPPROTO_IPV6		=	41;
inline int IPPROTO_RSVP		=	46;
inline int IPPROTO_GRE		=	47;
inline int IPPROTO_ESP		=	50;
inline int IPPROTO_AH		=	51;
inline int IPPROTO_OSPF		=	89;
inline int IPPROTO_MTP		=	92;
inline int IPPROTO_BEETPH	=	94;
inline int IPPROTO_ENCAP	=	98;
inline int IPPROTO_PIM		=	103;
inline int IPPROTO_COMP		=	108;
inline int IPPROTO_SCTP		=	132;
inline int IPPROTO_UDPLITE	=	136;
inline int IPPROTO_RAW		=	255;
inline int IPPROTO_MAX		=	256;
inline int IPPROTO_HOPOPTS	=	0;
inline int IPPROTO_ROUTING	=	43;
inline int IPPROTO_FRAGMENT	=	44;
inline int IPPROTO_ICMPV6	=	58;
inline int IPPROTO_NONE		=	59;
inline int IPPROTO_DSTOPTS	=	60;
inline int IPPROTO_MH		=	135;

inline int TCP_MIN_HEADER_LENGTH =	20;

/*
 * For compatibility with Solaris.  Here the netstackid will be the pointer
 * to the net namespace (nd_net in struct net_device).
 */
typedef uint64_t	netstackid_t;
typedef __be32		ipaddr_t;
typedef struct in6_addr	in6_addr_t;

/*
 * pktinfo is where packet ID info can be made available for deeper
 * analysis if packet IDs become supported by the kernel in the future.
 * The pkt_addr member is currently always NULL.
 */
typedef struct pktinfo {
	uintptr_t pkt_addr;
} pktinfo_t;

/*
 * csinfo is where connection state info is made available.
 */
typedef struct csinfo {
	uintptr_t cs_addr;
	uint64_t cs_cid;
} csinfo_t;

/*
 * ipinfo contains common IP info for both IPv4 and IPv6.
 */
typedef struct ipinfo {
	uint8_t ip_ver;			/* IP version (4, 6) */
	uint32_t ip_plength;		/* payload length */
	string ip_saddr;		/* source address */
	string ip_daddr;		/* destination address */
} ipinfo_t;

/*
 * ifinfo contains network interface info.
 */
typedef struct ifinfo {
	string if_name;			/* interface name */
	int8_t if_local;		/* is delivered locally */
	netstackid_t if_ipstack;	/* netns pointer on Linux */
	uintptr_t if_addr;		/* pointer to raw struct net_device */
} ifinfo_t;

/*
 * ipv4info is a translated version of the IPv4 header (with raw pointer).
 * These values are NULL if the packet is not IPv4.
 */
typedef struct ipv4info {
	uint8_t ipv4_ver;		/* IP version (4) */
	uint8_t ipv4_ihl;		/* header length, bytes */
	uint8_t ipv4_tos;		/* type of service field */
	uint16_t ipv4_length;		/* length (header + payload) */
	uint16_t ipv4_ident;		/* identification */
	uint8_t ipv4_flags;		/* IP flags */
	uint16_t ipv4_offset;		/* fragment offset */
	uint8_t ipv4_ttl;		/* time to live */
	uint8_t ipv4_protocol;		/* next level protocol */
	string ipv4_protostr;		/* next level protocol, as string */
	uint16_t ipv4_checksum;		/* header checksum */
	ipaddr_t ipv4_src;		/* source address */
	ipaddr_t ipv4_dst;		/* destination address */
	string ipv4_saddr;		/* source address, string */
	string ipv4_daddr;		/* destination address, string */
	struct iphdr *ipv4_hdr;		/* pointer to raw header */
} ipv4info_t;

/*
 * ipv6info is a translated version of the IPv6 header (with raw pointer).
 * These values are NULL if the packet is not IPv6.
 */
typedef struct ipv6info {
	uint8_t ipv6_ver;		/* IP version (6) */
	uint8_t ipv6_tclass;		/* traffic class */
	uint32_t ipv6_flow;		/* flow label */
	uint16_t ipv6_plen;		/* payload length */
	uint8_t ipv6_nexthdr;		/* next header protocol */
	string ipv6_nextstr;		/* next header protocol, as string */
	uint8_t ipv6_hlim;		/* hop limit */
	in6_addr_t *ipv6_src;		/* source address */
	in6_addr_t *ipv6_dst;		/* destination address */
	string ipv6_saddr;		/* source address, string */
	string ipv6_daddr;		/* destination address, string */
	struct ipv6hdr *ipv6_hdr;	/* pointer to raw header */
} ipv6info_t;

/*
 * void_ip_t is a void pointer to either an IPv4 or IPv6 header.  It has
 * its own type name so that a translator can be determined.
 */
typedef uintptr_t void_ip_t;

/*
 * __dtrace_tcp_void_ip_t is used by the translator to take either the
 * non-NULL void_ip_t * passed in or, if it is NULL, uses arg3 (struct tcp *)
 * from the tcp:::send probe to translate to an ipinfo_t.
 * This allows us to present the consumer with header data based on the
 * struct tcp * when IP information is not yet present (for TCP send).
 */
typedef void * __dtrace_tcp_void_ip_t;

#pragma D binding "1.5" translator
translator pktinfo_t < struct sk_buff *s > {
	pkt_addr = (uintptr_t)s;
};

#pragma D binding "1.5" translator
translator csinfo_t < struct sock *s > {
	cs_addr = (uintptr_t)s;
};

#pragma D binding "1.5" translator
translator ipinfo_t < struct iphdr *I > {
	ip_ver = 4;
        ip_plength = I != NULL ? (ntohs(I->tot_len) - I->ihl << 2) : 0;
	ip_saddr = I != NULL ? inet_ntoa(&I->saddr) : "<unknown>";
	ip_daddr = I != NULL ? inet_ntoa(&I->daddr) : "<unknown>";
};

#pragma D binding "1.5" translator
translator ipinfo_t < struct ipv6hdr *I > {
	ip_ver = 6;
	ip_plength = I != NULL ? ntohs(I->payload_len) : 0;
	ip_saddr = I != NULL ? inet_ntoa6(&I->saddr) : "<unknown>";
	ip_daddr = I != NULL ? inet_ntoa6(&I->daddr) : "<unknown>";
};

#pragma D binding "1.5" translator
translator ipinfo_t < void_ip_t *I > {
	ip_ver = I != NULL ? *(uint8_t *)I >> 4 : 0;
	ip_plength = I != NULL ? (*(uint8_t *)I >> 4 == 4 ?
	    (ntohs(((struct iphdr *)I)->tot_len) - ((*(uint8_t *)I & 0xf) << 2)) :
	    *(uint8_t *)I >> 4 == 6 ?
	    ntohs(((struct ipv6hdr *)I)->payload_len) : 0) : 0;
	ip_saddr = I != NULL ? (*(uint8_t *)I >> 4 == 4 ?
	    inet_ntoa(&((struct iphdr *)I)->saddr) : *(uint8_t *)I >> 4 == 6 ?
	    inet_ntoa6(&((struct ipv6hdr *)I)->saddr) : "<unknown>") :
	    "<unknown>";
	ip_daddr = I != NULL ? (*(uint8_t *)I >> 4 == 4 ?
	    inet_ntoa(&((struct iphdr *)I)->daddr) : *(uint8_t *)I >> 4 == 6 ?
	    inet_ntoa6(&((struct ipv6hdr *)I)->daddr) : "<unknown>") :
	    "<unknown>";
};

/*
 * In some cases where the ipinfo_t * is NULL we wish to construct IP info
 * using the struct tcp_sock * (arg3).  In order to map local IP to source
 * or destination IP address appropriately we need to check if the associated
 * data is inbound (NET_PROBE_INBOUND in arg7) or outbound (NET_PROBE_OUTBOUND);
 * the value is stored in arg7.  If inbound, we map the local IP address to
 * ip_daddr (destination), and if outbound it is mapped to ip_saddr.
 */
#pragma D binding "1.5" translator
translator ipinfo_t < __dtrace_tcp_void_ip_t *I > {
	/*
	 * General strategy used is to rely on the IP header I if it is
	 * non-null; otherwise we try to reconstruct IP values from arg3
	 * (a struct tcp_sock *).
	 */
	ip_ver = I != NULL ? *(uint8_t *)I >> 4 :
	    arg3 != NULL &&
	    ((struct sock *)arg3)->__sk_common.skc_family == AF_INET ? 4 :
	    arg3 != NULL &&
	    ((struct sock *)arg3)->__sk_common.skc_family== AF_INET6 ? 6 : 0;
	/*
	 * For ip_plength we fall back to using TCP skb data from the tcp_skb_cb
	 * to determine payload length.
	 */
	ip_plength = I != NULL && *(uint8_t *)I >> 4 == 4 ?
	    ntohs(((struct iphdr *)I)->tot_len) - ((*(uint8_t *)I & 0xf) << 2) :
	    I != NULL && *(uint8_t *)I >> 4 == 6 ?
	    ntohs(((struct ipv6hdr *)I)->payload_len) :
	    arg0 != NULL ?
	    ((struct tcp_skb_cb *)&(((struct sk_buff *)arg0)->cb[0]))->end_seq -
	    ((struct tcp_skb_cb *)&(((struct sk_buff *)arg0)->cb[0]))->seq :
	    0;
	/*
	 * For source/destination addresses, we again try to use the IP header I
	 * if available.  If I is NULL, we utilize arg3 (struct tcp_sock *)
	 * but the problem here is that it stores local and remote IP addresses
	 * _not_ source and destination.  So we need to know if traffic is
	 * inbound or outbound. If inbound, IP source address is remote
	 * socket address (skc_daddr) and destination IP address is local socket
	 * address (skc_rcv_saddr).  If outbound, IP source address is local
	 * socket address and IP destination address is remote socket address.
	 */
	ip_saddr = I != NULL && *(uint8_t *)I >> 4 == 4 ?
	    inet_ntoa(&((struct iphdr *)I)->saddr) :
	    I != NULL && *(uint8_t *)I >> 4 == 6 ?
	    inet_ntoa6(&((struct ipv6hdr *)I)->saddr) :
	    arg3 != NULL &&
	    ((struct sock *)arg3)->__sk_common.skc_family== AF_INET ?
	    inet_ntoa(arg7 == NET_PROBE_INBOUND ?
	    &((struct sock *)arg3)->__sk_common.skc_daddr :
	    &((struct sock *)arg3)->__sk_common.skc_rcv_saddr) :
	    arg3 != NULL &&
	    ((struct sock *)arg3)->__sk_common.skc_family == AF_INET6 ?
	    inet_ntoa6(arg7 == NET_PROBE_INBOUND ?
	    &((struct sock *)arg3)->__sk_common.skc_v6_daddr :
	    &((struct sock *)arg3)->__sk_common.skc_v6_rcv_saddr) :
	    "<unknown>";
	ip_daddr = I != NULL && *(uint8_t *)I >> 4 == 4 ?
	    inet_ntoa(&((struct iphdr *)I)->daddr) :
	    I != NULL && *(uint8_t *)I >> 4 == 6 ?
	    inet_ntoa6(&((struct ipv6hdr *)I)->daddr) :
	    arg3 != NULL &&
	    ((struct sock *)arg3)->__sk_common.skc_family== AF_INET ?
	    inet_ntoa(arg7 == NET_PROBE_INBOUND ?
	    &((struct sock *)arg3)->__sk_common.skc_rcv_saddr :
	    &((struct sock *)arg3)->__sk_common.skc_daddr) :
	    arg3 != NULL &&
	    ((struct sock *)arg3)->__sk_common.skc_family== AF_INET6 ?
	    inet_ntoa6(arg7 == NET_PROBE_INBOUND ?
	    &((struct sock *)arg3)->__sk_common.skc_v6_rcv_saddr :
	    &((struct sock *)arg3)->__sk_common.skc_v6_daddr) :
	    "<unknown>";
};

#pragma D binding "1.5" translator
translator ifinfo_t < struct net_device *N > {
	if_name = N != NULL ? stringof(N->name) : "<unknown>";
	if_ipstack = (N != NULL && sizeof(N->nd_net) > 0) ?
	    ((uint64_t)N->nd_net.net) : 0;
	if_local = (N == NULL);	/* is delivered locally */
	if_addr = (uintptr_t)N;	/* pointer to raw struct net_device ptr */
};

#pragma D binding "1.5" translator
translator ipv4info_t < struct iphdr *I > {
	ipv4_ver = I != NULL ? 4 : 0;
	ipv4_ihl = I != NULL ? ((*(uint8_t *)I & 0xf) << 2) : 0;
	ipv4_tos = I != NULL ? I->tos : 0;
	ipv4_length = I != NULL ? ntohs(I->tot_len) : 0;
	ipv4_ident = I != NULL ? ntohs(I->id) : 0;
	ipv4_flags = I != NULL ? ntohs(I->frag_off) >> 12 : 0;
	ipv4_offset = I != NULL ? ntohs(I->frag_off) & 0x0fff : 0;
	ipv4_ttl = I != NULL ? I->ttl : 0;
	ipv4_protocol = I != NULL ? I->protocol : 0;
	ipv4_protostr = I == NULL ? "<null>" :
	    I->protocol == IPPROTO_TCP     ? "TCP"    :
	    I->protocol == IPPROTO_UDP     ? "UDP"    :
	    I->protocol == IPPROTO_IP      ? "IP"     :
	    I->protocol == IPPROTO_ICMP    ? "ICMP"   :
	    I->protocol == IPPROTO_IGMP    ? "IGMP"   :
	    I->protocol == IPPROTO_EGP     ? "EGP"    :
	    I->protocol == IPPROTO_IPV6    ? "IPv6"   :
	    I->protocol == IPPROTO_ROUTING ? "ROUTE"  :
	    I->protocol == IPPROTO_ESP     ? "ESP"    :
	    I->protocol == IPPROTO_AH      ? "AH"     :
	    I->protocol == IPPROTO_ICMPV6  ? "ICMPv6" :
	    I->protocol == IPPROTO_OSPF    ? "OSPF"   :
	    I->protocol == IPPROTO_SCTP    ? "SCTP"   :
	    I->protocol == IPPROTO_RAW     ? "RAW"    :
	    lltostr((uint64_t)I->protocol);
	ipv4_checksum = I != NULL ? ntohs(I->check) : 0;
	ipv4_src = I != NULL ? I->saddr : 0;
	ipv4_dst = I != NULL ? I->daddr : 0;
	ipv4_saddr = I != NULL ? inet_ntoa(&I->saddr) : "<null>";
	ipv4_daddr = I != NULL ? inet_ntoa(&I->daddr) : "<null>";
	ipv4_hdr = I;
};

#pragma D binding "1.5" translator
translator ipv6info_t < struct ipv6hdr *I > {
	ipv6_ver = I != NULL ? ((*(uint8_t *)I) >> 4) : 0;
	ipv6_tclass = I != NULL ? ((ntohl(*(uint32_t *)I) & 0x0fffffff) >> 20) : 0;
	ipv6_flow = I != NULL ? (ntohl(*(uint32_t *)I) & 0x000fffff) : 0;
	ipv6_plen = I != NULL ? ntohs(I->payload_len) : 0;
	ipv6_nexthdr = I != NULL ? I->nexthdr : 0;
	ipv6_nextstr = I == NULL ? "<null>" :
	    I->nexthdr == IPPROTO_TCP	?	"TCP"		:
	    I->nexthdr == IPPROTO_UDP	?	"UDP"		:
	    I->nexthdr == IPPROTO_IP	?	"IP"		:
	    I->nexthdr == IPPROTO_ICMP	?	"ICMP"		:
	    I->nexthdr == IPPROTO_IGMP	?	"IGMP"		:
	    I->nexthdr == IPPROTO_EGP	?	"EGP"		:
	    I->nexthdr == IPPROTO_IPV6	?	"IPv6"		:
	    I->nexthdr == IPPROTO_ROUTING ?	"ROUTE"		:
	    I->nexthdr == IPPROTO_ESP	?	"ESP"		:
	    I->nexthdr == IPPROTO_AH	?	"AH"		:
	    I->nexthdr == IPPROTO_ICMPV6 ?	"ICMPv6"	:
	    I->nexthdr == IPPROTO_OSPF ?	"OSPF"		:
	    I->nexthdr == IPPROTO_SCTP	?	"SCTP"		:
	    I->nexthdr == IPPROTO_RAW	?	"RAW"		:
	    lltostr((uint64_t)I->nexthdr);
	ipv6_hlim = I != NULL ? I->hop_limit : 0;
	ipv6_src = I != NULL ? &I->saddr : NULL;
	ipv6_dst = I != NULL ? &I->daddr : 0;
	ipv6_saddr = I != NULL ? inet_ntoa6(&I->saddr) : "<null>";
	ipv6_daddr = I != NULL ? inet_ntoa6(&I->daddr) : "<null>";
	ipv6_hdr = I;
};
