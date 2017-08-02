/*
 * Oracle Linux DTrace.
 * Copyright © 2013, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _SPARC_PLATFORM_H
#define _SPARC_PLATFORM_H

#include <inttypes.h>
#include <asm/ptrace.h>
#include <elf.h>
#include <sys/syscall.h>			/* for __NR_* */

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

