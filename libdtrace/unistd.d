/*
 * Oracle Linux DTrace.
 * Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

inline int DTRACEFLT_UNKNOWN = 0;	/* Unknown fault */
#pragma D binding "1.0" DTRACEFLT_UNKNOWN

inline int DTRACEFLT_BADADDR = 1;	/* Bad address */
#pragma D binding "1.0" DTRACEFLT_BADADDR

inline int DTRACEFLT_BADALIGN = 2;	/* Bad alignment */
#pragma D binding "1.0" DTRACEFLT_BADALIGN

inline int DTRACEFLT_ILLOP = 3;		/* Illegal operation */
#pragma D binding "1.0" DTRACEFLT_ILLOP

inline int DTRACEFLT_DIVZERO = 4;	/* Divide-by-zero */
#pragma D binding "1.0" DTRACEFLT_DIVZERO

inline int DTRACEFLT_NOSCRATCH = 5;	/* Out of scratch space */
#pragma D binding "1.0" DTRACEFLT_NOSCRATCH

inline int DTRACEFLT_KPRIV = 6;		/* Illegal kernel access */
#pragma D binding "1.0" DTRACEFLT_KPRIV

inline int DTRACEFLT_UPRIV = 7;		/* Illegal user access */
#pragma D binding "1.0" DTRACEFLT_UPRIV

inline int DTRACEFLT_TUPOFLOW = 8;	/* Tuple stack overflow */
#pragma D binding "1.0" DTRACEFLT_TUPOFLOW

inline int DTRACEFLT_BADSTACK = 9;	/* Bad stack */
#pragma D binding "1.4.1" DTRACEFLT_BADSTACK
