// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 */

#define BPF_FUNC_probe_read	4
#define BPF_FUNC_probe_read_str	45

/*
 * uint64_t dt_strchr(char *src, uint64_t c, char *dst, char *tmp) {
 *
 *     // make a copy of the char in every byte of the register
 *     c &= 0xff;
 *     c |= (c << 8);
 *     c |= (c << 16);
 *     c |= (c << 32);
 *
 *     // spill arguments to stack
 *     [%fp-8]=src
 *     [%fp-16]=c
 *     [%fp-24]=dst
 *     [%fp-32]=tmp
 *
 *     // make temporary copy of string and get string length
 *     r6 = bpf_probe_read_str(dst, STRSZ, src);
 *     r6--;
 *
 *     // xor the char with every byte;  a match results in NULL byte
 *     r4 = roundup(r6, 8);
 *     for (r3 = 0; r3 < r4; r3 += 8)
 *         ((uint64_t *)dst)[r3] ^= c;
 *
 *     // put a safeguard in place, then look for that NULL byte
 *     dst[r6] = '\0';
 *     r8 = bpf_probe_read_str(tmp, r6 + 1, dst);
 *     r8--;
 *
 *     // determine length of output string
 *     r6 -= r8;
 *     if (r6 <= 0) return -1;
 *
 *     // write output string
 *     bpf_probe_read(dst, r6, src + r8);
 *     dst[r6] = '\0';
 *
 *     return 0;
 * }
 */
	.text
	.align	4
	.global	dt_strchr
	.type	dt_strchr, @function
dt_strchr :
	and	%r2, 0xff		/* c &= 0xff */
	mov	%r5, %r2
	lsh	%r5, 8
	or	%r2, %r5		/* c |= (c << 8) */
	mov	%r5, %r2
	lsh	%r5, 16
	or	%r2, %r5		/* c |= (c << 16) */
	mov	%r5, %r2
	lsh	%r5, 32
	or	%r2, %r5		/* c |= (c << 32) */

	stxdw	[%fp+-8], %r1		/* Spill src */
	stxdw	[%fp+-16], %r2		/* Spill c */
	stxdw	[%fp+-24], %r3		/* Spill dst */
	stxdw	[%fp+-32], %r4		/* Spill tmp */

	ldxdw	%r1, [%fp+-24]
	lddw	%r2, STRSZ
	ldxdw	%r3, [%fp+-8]
	call	BPF_FUNC_probe_read_str	/* r6 = bpf_probe_read_str(dst, STRSZ, src) */
	mov	%r6, %r0

	jsle	%r6, 0, .Lerror

	sub	%r6, 1			/* r6-- */

	mov	%r4, %r6		/* r4 = roundup(r6, 8) */
	add	%r4, 7
	and	%r4, -8

	ldxdw	%r1, [%fp+-16]
	mov	%r3, 0
.Lloop:					/* for (r3 = 0; r3 < r4; r3 += 8) */
	ldxdw	%r5, [%fp+-24]
	add	%r5, %r3
	ldxdw	%r0, [%r5+0]
	xor	%r0, %r1		/* 	((uint64_t *)dst)[r3] ^= c; */
	stxdw	[%r5+0], %r0
	add	%r3, 8
	jlt	%r3, %r4, .Lloop

	ldxdw	%r2, [%fp+-24]
	add	%r2, %r6
	mov	%r0, 0
	stxb	[%r2+0], %r0		/* dst[r6] = '\0' */

	ldxdw	%r1, [%fp+-32]
	mov	%r2, %r6
	add	%r2, 1
	ldxdw	%r3, [%fp+-24]
	call	BPF_FUNC_probe_read_str	/* r8 = bpf_probe_read_str(tmp, r6 + 1, dst) */
	jsle	%r0, 0, .Lerror
	lsh	%r0, 32
	arsh	%r0, 32
	mov	%r8, %r0

	add	%r8, -1			/* r8-- */

	sub	%r6, %r8		/* r6 -= r8 */

	jsle	%r6, 0, .Lerror		/* if (r6 <= 0) return -1 */

	ldxdw	%r1, [%fp+-24]
	mov	%r2, %r6
	ldxdw	%r3, [%fp+-8]
	add	%r3, %r8
	call	BPF_FUNC_probe_read	/* bpf_probe_read(dst, r6, src + r8) */

	ldxdw	%r1, [%fp+-24]
	add	%r1, %r6		/* dst[r6] = '\0' */
	mov	%r2, 0
	stxb	[%r1+0], %r2

	mov	%r0, 0			/* return 0 */
	exit

.Lerror:
	mov	%r0, -1
	exit
	.size	dt_strchr, .-dt_strchr
