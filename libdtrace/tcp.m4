/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright (c) 2010, 2012, 2013 Oracle and/or its affiliates.
 * All rights reserved.
 */
/*
#include <inet/tcp.h>
#include <sys/netstack.h>
*/
#define	DEF_REPLACE(x)	__def_replace(#x,x)

DEF_REPLACE(TH_FIN)
DEF_REPLACE(TH_SYN)
DEF_REPLACE(TH_RST)
DEF_REPLACE(TH_PUSH)
DEF_REPLACE(TH_ACK)
DEF_REPLACE(TH_URG)
DEF_REPLACE(TH_ECE)
DEF_REPLACE(TH_CWR)

DEF_REPLACE(TCPS_CLODEF)
DEF_REPLACE(TCPS_IDLE)
DEF_REPLACE(TCPS_BOUND)
DEF_REPLACE(TCPS_LISTEN)
DEF_REPLACE(TCPS_SYN_SENT)
DEF_REPLACE(TCPS_SYN_RCVD)
DEF_REPLACE(TCPS_ESTABLISHED)
DEF_REPLACE(TCPS_CLOSE_WAIT)
DEF_REPLACE(TCPS_FIN_WAIT_1)
DEF_REPLACE(TCPS_CLOSING)
DEF_REPLACE(TCPS_LAST_ACK)
DEF_REPLACE(TCPS_FIN_WAIT_2)
DEF_REPLACE(TCPS_TIME_WAIT)

DEF_REPLACE(TCP_MIN_HEADER_LENGTH)
