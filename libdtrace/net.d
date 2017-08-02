/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright 2007, 2012, 2013, 2017 Oracle, Inc.  All rights reserved.
 */

/* These are needed for inet_ntop() DTrace function. */
inline int AF_INET              =       2;
inline int AF_INET6             =       10;

/* These are needed for link_ntop() DTrace function. */
inline int ARPHRD_ETHER         =       1;
inline int ARPHRD_INFINIBAND    =       32;

/*
 * The conninfo_t structure should be used by all application protocol
 * providers as the first arguments to indicate some basic information
 * about the connection. This structure may be augmented to accommodate
 * the particularities of additional protocols in the future.
 */
typedef struct conninfo {
	 string ci_local;	/* local host address */
	 string ci_remote;	/* remote host address */
	 string ci_protocol;	/* protocol (ipv4, ipv6, etc) */
} conninfo_t;

/*
 * We use these values to determine if a probe point is associated
 * with sending (outbound) or receiving (inbound).
 */
inline int NET_PROBE_OUTBOUND =		0x00;
inline int NET_PROBE_INBOUND =		0x01;
