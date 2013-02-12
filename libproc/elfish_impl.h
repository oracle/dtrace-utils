/*
 * ELF-related support code, bitness-dependent prototypes.
 */

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
 * Copyright 2012 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file can be repeatedly #included with different values of BITS.  Hence,
 * no include guards.
 */

#include <inttypes.h>
#include "Pcontrol.h"

#ifndef BITS
#error BITS must be set before including elfish_impl.h
#endif

#ifndef BITIZE
#define JOIN(pre,post) pre##_elf##post
#define EXJOIN(pre,post) JOIN(pre,post)
#define BITIZE(pre) EXJOIN(pre,BITS)
#endif

/*
 * Prototypes for this value of BITS.
 */

extern uintptr_t BITIZE(r_debug)(struct ps_prochandle *P);

#undef JOIN
#undef EXJOIN
#undef BITIZE
