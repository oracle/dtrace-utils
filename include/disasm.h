/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DISASM_H
#define _DISASM_H

/*
 * An interface to BFD dis-asm.h.  This is difficult to #include because
 * it includes headers that require bfd's config.h to be included first,
 * which is not an installed file.  It's unnecessary on any platform
 * DTrace works on, so we fool BFD into thinking it's installed here.
 */

#define PACKAGE fool-me-once
#define PACKAGE_VERSION 0.666

#include <dis-asm.h>

#undef PACKAGE
#undef PACKAGE_VERSION

#endif
