/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* These are needed for inet_ntop() DTrace function. */
inline int AF_INET              =       2;
inline int AF_INET6             =       10;

/* These are needed for link_ntop() DTrace function. */
inline int ARPHRD_ETHER         =       1;
inline int ARPHRD_INFINIBAND    =       32;

/*
 * The conninfo_t structure should be used by all application protocol
 * providers as the first arguments to indicate some basic information about
 * the connection. This structure may be augmented to accommodate the
 * particularities of additional protocols in the future.
 */
typedef struct conninfo {
	string	ci_local;		/* local host address */
	string	ci_remote;		/* remote host address */
	string	ci_protocol;		/* protocol (ipv4, ipv6, etc) */
} conninfo_t;

/*
 * For compatibility with Solaris.  Here the netstackid will be the pointer to
 * the net namespace (nd_net in struct net_device).
 */
typedef uint64_t	netstackid_t;
typedef __be32		ipaddr_t;
typedef struct in6_addr	in6_addr_t;

/*
 * pktinfo is where packet ID info can be made available for deeper analysis if
 * packet IDs become supported by the kernel in the future.
 * The pkt_addr member is currently always NULL.
 */
typedef struct pktinfo {
	uintptr_t	pkt_addr;
} pktinfo_t;

/*
 * csinfo is where connection state info is made available.
 */
typedef struct csinfo {
	uintptr_t	cs_addr;
	uint64_t	cs_cid;
} csinfo_t;

/*
 * We use these values to determine if a probe point is associated with sending
 * (outbound) or receiving (inbound).
 */
inline int NET_PROBE_OUTBOUND =		0x00;
inline int NET_PROBE_INBOUND =		0x01;
