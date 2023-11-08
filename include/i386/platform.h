/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _I386_PLATFORM_H
#define _I386_PLATFORM_H

#include <inttypes.h>
#include <sys/reg.h>
#include <sys/syscall.h>			/* for __NR_* */

/*
 * Must be no larger than an 'unsigned long'.
 */
const static unsigned char plat_bkpt[] = { 0xcc };

/*
 * Number of processor-specific dynamic tags on this platform.
 */
#define DT_THISPROCNUM 0

/*
 * TRUE if this platform requires software singlestepping.
 */
#undef NEED_SOFTWARE_SINGLESTEP

#endif

