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
 * Copyright 2009 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_ELF_AMD64_H
#define	_SYS_ELF_AMD64_H

#include <sys/elf_386.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * There are consumers of this file that want to include elf defines for
 * all architectures.  This is a problem for the defines below, because
 * while they are architecture specific they have common names.  Hence to
 * prevent attempts to redefine these variables we'll check if any of
 * the other elf architecture header files have been included.  If
 * they have then we'll just stick with the existing definitions.
 */
#if defined(_SYS_ELF_MACH_386)

/*
 * Plt and Got information; the first few .got and .plt entries are reserved
 *	PLT[0]	jump to dynamic linker
 *	GOT[0]	address of _DYNAMIC
 */
#define	M64_WORD_ALIGN		8
#define	M64_PLT_ENTSIZE		M32_PLT_ENTSIZE
#define	M64_PLT_ALIGN		M64_WORD_ALIGN	/* alignment of .plt section */
#define	M64_GOT_ENTSIZE		8	/* got entry size in bytes */
#define	M64_PLT_RESERVSZ	M32_PLT_RESERVSZ

/*
 * Make common alias for the 32/64 bit specific defines based on _ELF64
 */
#if defined(_ELF64)
/* architecture common defines */
#define	M_WORD_ALIGN		M64_WORD_ALIGN
#define	M_PLT_ENTSIZE		M64_PLT_ENTSIZE
#define	M_PLT_ALIGN		M64_PLT_ALIGN
#define	M_PLT_RESERVSZ		M64_PLT_RESERVSZ
#define	M_GOT_ENTSIZE		M64_GOT_ENTSIZE
#endif /* _ELF64 */

#endif /* _SYS_ELF_MACH_386 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELF_AMD64_H */
