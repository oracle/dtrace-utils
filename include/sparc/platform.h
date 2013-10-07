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
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SPARC_PLATFORM_H
#define _SPARC_PLATFORM_H

#include <inttypes.h>
#include <asm/ptrace.h>
#include <elf.h>

/*
 * Must be no larger than an 'unsigned long'.
 */
const static unsigned char plat_bkpt[] = { 0x91, 0xd0, 0x20, 0x01 };

/*
 * Number of processor-specific dynamic tags on this platform.
 */
#define DT_THISPROCNUM DT_SPARC_NUM

/*
 * TRUE if this platform requires software singlestepping.
 */
#define NEED_SOFTWARE_SINGLESTEP 1

#endif

