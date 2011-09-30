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
 * Copyright 2011 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef PROCESSOR_H
#define PROCESSOR_H
typedef unsigned int processorid_t;

/*
 * Flags and return values for p_online(2), and pi_state for processor_info(2).
 * These flags are *not* for in-kernel examination of CPU states.
 * See <sys/cpuvar.h> for appropriate informational functions.
 */
#define	P_OFFLINE	0x0001	/* processor is offline, as quiet as possible */
#define	P_ONLINE	0x0002	/* processor is online */
#define	P_STATUS	0x0003	/* value passed to p_online to request status */
#define	P_FAULTED	0x0004	/* processor is offline, in faulted state */
#define	P_POWEROFF	0x0005	/* processor is powered off */
#define	P_NOINTR	0x0006	/* processor is online, but no I/O interrupts */
#define	P_SPARE		0x0007	/* processor is offline, can be reactivated */
#define	P_BAD		P_FAULTED	/* unused but defined by USL */
#define	P_FORCED 	0x10000000	/* force processor offline */

#endif

