/*
 * ELF-related support code, bitness-dependent prototypes.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
