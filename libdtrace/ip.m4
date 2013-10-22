/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2007, 2010, 2012, Oracle and/or its affiliates.
 * All rights reserved.
 */

/*
 * This file is an m4 script which is first preprocessed by cpp or cc -E to
 * substitute #define tokens for their values. It is then run over ip.d.in
 * to replace those tokens with their values to create the finished ip.d.
 */

/*
#include <sys/netstack.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <inet/ip.h>
#include <inet/tcp.h>
*/
#define	DEF_REPLACE(x)	__def_replace(#x,x)

DEF_REPLACE(AF_INET)
DEF_REPLACE(AF_INET6)

DEF_REPLACE(IPH_DF)
DEF_REPLACE(IPH_MF)

DEF_REPLACE(IPPROTO_IP)
DEF_REPLACE(IPPROTO_HOPOPTS)
DEF_REPLACE(IPPROTO_ICMP)
DEF_REPLACE(IPPROTO_IGMP)
DEF_REPLACE(IPPROTO_GGP)
DEF_REPLACE(IPPROTO_ENCAP)
DEF_REPLACE(IPPROTO_TCP)
DEF_REPLACE(IPPROTO_EGP)
DEF_REPLACE(IPPROTO_PUP)
DEF_REPLACE(IPPROTO_UDP)
DEF_REPLACE(IPPROTO_IDP)
DEF_REPLACE(IPPROTO_IPV6)
DEF_REPLACE(IPPROTO_ROUTING)
DEF_REPLACE(IPPROTO_FRAGMENT)
DEF_REPLACE(IPPROTO_RSVP)
DEF_REPLACE(IPPROTO_ESP)
DEF_REPLACE(IPPROTO_AH)
DEF_REPLACE(IPPROTO_ICMPV6)
DEF_REPLACE(IPPROTO_NONE)
DEF_REPLACE(IPPROTO_DSTOPTS)
DEF_REPLACE(IPPROTO_HELLO)
DEF_REPLACE(IPPROTO_ND)
DEF_REPLACE(IPPROTO_EON)
DEF_REPLACE(IPPROTO_OSPF)
DEF_REPLACE(IPPROTO_PIM)
DEF_REPLACE(IPPROTO_SCTP)
DEF_REPLACE(IPPROTO_RAW)
DEF_REPLACE(IPPROTO_MAX)

DEF_REPLACE(TCP_MIN_HEADER_LENGTH)

DEF_REPLACE(GLOBAL_NETSTACKID)
