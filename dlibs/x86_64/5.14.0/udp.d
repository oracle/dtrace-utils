/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on library net.d
#pragma D depends_on provider udp

/*
 * udpinfo is the UDP header fields.
 */
typedef struct udpinfo {
	uint16_t udp_sport;		/* source port */
	uint16_t udp_dport;		/* destination port */
	uint16_t udp_length;		/* total length */
	uint16_t udp_checksum;          /* headers + data checksum */
	struct udphdr *udp_hdr;		/* raw UDP header */
} udpinfo_t;

/*
 * udpsinfo contains stable UDP details from udp_t.
 */
typedef struct udpsinfo {
	uintptr_t udps_addr;
	uint16_t udps_lport;		/* local port */
	uint16_t udps_rport;		/* remote port */
	string udps_laddr;		/* local address, as a string */
	string udps_raddr;		/* remote address, as a string */
} udpsinfo_t;

#pragma D binding "1.6.3" translator
translator udpinfo_t < struct udphdr *U > {
	udp_sport = ntohs(U->source);
	udp_dport = ntohs(U->dest);
	udp_length = ntohs(U->len);
	udp_checksum = ntohs(U->check);
	udp_hdr = U;
};

#pragma D binding "1.6.3" translator
translator udpsinfo_t < struct udp_sock *S > {
	/*
	 * We source udp info from other args but retain the sock arg here
	 * as it may be used in the future.
	 */
	udps_addr = (uintptr_t)S;
	udps_lport = arg4 ?
	    (probename == "send" ? ntohs(((struct udphdr *)arg4)->source) :
	    ntohs(((struct udphdr *)arg4)->dest)) : 0;
	udps_rport = arg4 ?
	    (probename == "send" ? ntohs(((struct udphdr *)arg4)->dest) :
	    ntohs(((struct udphdr *)arg4)->source)) : 0;
	udps_laddr = arg2 && *(uint8_t *)arg2 >> 4 == 4 ?
            inet_ntoa(probename == "send" ? &((struct iphdr *)arg2)->saddr :
	    &((struct iphdr *)arg2)->daddr) :
	    arg2 && *(uint8_t *)arg2 >> 4 == 6 ?
	    inet_ntoa6(probename == "send" ? &((struct ipv6hdr *)arg2)->saddr :
	    &((struct ipv6hdr *)arg2)->daddr) :
	    "<unknown>";
	udps_raddr =
	    arg2 && *(uint8_t *)arg2 >> 4 == 4 ?
            inet_ntoa(probename == "send" ? &((struct iphdr *)arg2)->daddr :
	    &((struct iphdr *)arg2)->saddr) :
	    arg2 && *(uint8_t *)arg2 >> 4 == 6 ?
	    inet_ntoa6(probename == "send" ? &((struct ipv6hdr *)arg2)->daddr :
	    &((struct ipv6hdr *)arg2)->saddr) :
	    "<unknown>";
};
