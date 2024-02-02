// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>

/*
 * uint64_t dt_strrchr(char *src, uint64_t c, char *dst) {
 *
 *     c <<= 56;
 *
 *     [%fp-8]=src
 *     [%fp-16]=c
 *     [%fp-24]=dst
 *
 *     r6 = bpf_probe_read_str(dst, STRSZ, src);
 *     r6--;
 *
 *     r8 = r6;
 * Lloop:
 *     r8--;
 *     r3 = dst[r8];     // use copy in dst[] in case src[] is unsafe memory
 *     r3 <<= 56;
 *     if (r3 == c) goto Lfound;
 *     if (r8 > 0) goto Lloop;
 *
 * Lnone:
 *     return -1;
 *
 * Lfound:
 *     r6 -= r8;
 *     bpf_probe_read(dst, r6, src + r8);
 *
 *     dst[r6] = '\0';
 *
 *     return 0;
 * }
 */
	.text
	.align	4
	.global	dt_strrchr
	.type	dt_strrchr, @function
dt_strrchr :
	lsh	%r2, 56			/* c <<= 56 */

	stxdw	[%fp+-8], %r1		/* Spill src */
	stxdw	[%fp+-16], %r2		/* Spill c */
	stxdw	[%fp+-24], %r3		/* Spill dst */

	ldxdw	%r1, [%fp+-24]
	lddw	%r2, STRSZ
	ldxdw	%r3, [%fp+-8]
	call	BPF_FUNC_probe_read_str	/* r6 = bpf_probe_read_str(dst, STRSZ, src) */
	jsle	%r0, 0, .Lnone
	mov	%r6, %r0

	sub	%r6, 1			/* r6-- */

	mov	%r8, %r6		/* r8 = r6 */

	ldxdw	%r5, [%fp+-16]
	jsle	%r8, 0, .Lnone
.Lloop:
	sub	%r8, 1			/* r8-- */
	mov	%r4, %r8
	ldxdw	%r3, [%fp+-24]
	add	%r3, %r4
	ldxb	%r3, [%r3+0]		/* r3 = dst[r8] */
	lsh	%r3, 56			/* r3 <<= 56 */
	jeq	%r3, %r5, .Lfound	/* if (r3 == c) goto Lfound */
	jgt	%r8, 0, .Lloop		/* if (r8 > 0) goto Lloop */

.Lnone:
	mov	%r0, -1			/* return -1 */
	exit

.Lfound:
	sub	%r6, %r8		/* r6 -= r8 */
	jsle	%r6, 0, .Lnone

	ldxdw	%r1, [%fp+-24]
	mov	%r2, %r6
	ldxdw	%r3, [%fp+-8]
	add	%r3, %r8
	call	BPF_FUNC_probe_read	/* bpf_probe_read(dst, r6, src + r8) */

	ldxdw	%r1, [%fp+-24]
	add	%r1, %r6		/* dst[r6] = '\0' */
	mov	%r2, 0
	stxb	[%r1+0], %r2

	mov	%r0, 0		/* return 0 */
	exit
	.size	dt_strrchr, .-dt_strrchr
