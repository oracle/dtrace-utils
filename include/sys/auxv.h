/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#ifndef	_SYS_AUXV_H
#define	_SYS_AUXV_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * A bitness-independent auxv representation.
 */
typedef struct
{
	uint64_t a_type;
	union {
		uint64_t a_val;
		void *a_ptr;
	} a_un;
} auxv_t;

#ifdef	__cplusplus
}
#endif
#endif	/* _SYS_AUXV_H */
