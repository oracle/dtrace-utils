// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>

	.text

/*
 * void dt_strcmp_xor(char *s, const char *t, uint64_t len) {
 *     for (r6 = 0; r6 < len; r6++)
 *         s[r6] ^= t[r6];
 * }
 * where len is a positive multiple of 8.
 */
	.align	4
	.global	dt_strcmp_xor
	.type	dt_strcmp_xor, @function
dt_strcmp_xor :
	mov	%r6, 0
.Lxor:
	ldxdw	%r4, [%r1+0]
	ldxdw	%r5, [%r2+0]

	xor	%r4, %r5
	stxdw	[%r1+0], %r4

	add	%r1, 8
	add	%r2, 8
	add	%r6, 8
	jlt	%r6, %r3, .Lxor
	exit
	.size	dt_strcmp_xor, .-dt_strcmp_xor

/*
 * void dt_strcmp_not(char *s, uint64_t len) {
 *     for (r6 = 0; r6 < len; r6++)
 *         s[r6] = ! s[r6]
 * }
 */
	.align	4
	.global	dt_strcmp_not
	.type	dt_strcmp_not, @function
dt_strcmp_not :
	mov	%r6, 0
.Lnot:
	ldxb	%r3, [%r1+0]
	and	%r3, 0xff
	sub	%r3, 1
	rsh	%r3, 63
	stxb	[%r1+0], %r3
	add	%r1, 1
	add	%r6, 1
	jlt	%r6, %r2, .Lnot
	exit
	.size	dt_strcmp_not, .-dt_strcmp_not

/*
 * int dt_strcmp(char *s, char *t, char *tmp1, char *tmp2, uint64_t flags) {
 *
 *     [%fp-8]=s
 *     [%fp-16]=t
 *     [%fp-24]=tmp1
 *     [%fp-32]=tmp2
 *     r9=flags
 *
 *     r8 = STRSZ
 *
 *     // make temporary copies of strings
 *     r6 = bpf_probe_read_str(tmp1, STRSZ, s);
 *     r7 = bpf_probe_read_str(tmp2, STRSZ, t);
 *     tmp1[r6] = '\0';
 *     tmp2[r7] = '\0';
 *
 *     // round r8 up to a multiple of 8
 *     r8 = (r8 + 7) & -8;
 *
 *     // xor strings together, "not" bytes, and find first NULL
 *     // (this gives us the first byte that differs in the two strings)
 *     dt_strcmp_xor(tmp1, tmp2, r8);
 *     dt_strcmp_not(tmp1, r8);
 *     r0 = bpf_probe_read_str(tmp1, r8, tmp1);
 *     r0 -= 1;
 *
 *     // based on this location, judge if the strings are >, <, or ==
 *     if (r0 > r6) goto Lsame;
 *     if (r0 > r7) goto Lsame;
 *
 *     r7 = tmp2[r0];
 *
 *     bpf_probe_read(tmp1, 1, &s[r0]);
 *     r6 = tmp1[0];
 *
 *     // if all chars are the same, break tie on string length
 * Lsame:
 *     if (r6 > r7) return +1
 *     if (r6 < r7) return -1
 *     return 0;
 * }
 */
	.align	4
	.global	dt_strcmp
	.type	dt_strcmp, @function
dt_strcmp :

	stxdw	[%fp+-8], %r1		/* Spill s */
	stxdw	[%fp+-16], %r2		/* Spill t */
	stxdw	[%fp+-24], %r3		/* Spill tmp1 */
	stxdw	[%fp+-32], %r4		/* Spill tmp2 */
	mov	%r9, %r5		/* flags */

	lddw	%r8, STRSZ		/* r8 = STRSZ */

	ldxdw	%r1, [%fp+-24]
	mov	%r2, %r8
	ldxdw	%r3, [%fp+-8]
	call	BPF_FUNC_probe_read_str	/* r6 = bpf_probe_read_str(tmp1, STRSZ, s) */
	mov	%r6, %r0
	jle	%r6, %r8, 1
	mov	%r6, %r8

	ldxdw	%r1, [%fp+-32]
	mov	%r2, %r8
	ldxdw	%r3, [%fp+-16]
	call	BPF_FUNC_probe_read_str	/* r7 = bpf_probe_read_str(tmp2, STRSZ, t) */
	mov	%r7, %r0
	jle	%r7, %r8, 1
	mov	%r7, %r8

	mov	%r2, 0
	ldxdw	%r1, [%fp+-24]
	add	%r1, %r6
	stxb	[%r1+0], %r2		/* tmp1[r6] = '\0' */
	ldxdw	%r1, [%fp+-32]
	add	%r1, %r7
	stxb	[%r1+0], %r2		/* tmp2[r7] = '\0' */

	add	%r8, 7			/* round r8 up to a multiple of 8 */
	and	%r8, -8

	ldxdw	%r1, [%fp+-24]
	ldxdw	%r2, [%fp+-32]
	mov	%r3, %r8
	call dt_strcmp_xor		/* dt_strcmp_xor(tmp1, tmp2, r8) */
	ldxdw	%r1, [%fp+-24]
	mov	%r2, %r8
	call dt_strcmp_not		/* dt_strcmp_not(tmp1, r8) */

	ldxdw	%r1, [%fp+-24]
	mov	%r2, %r8
	ldxdw	%r3, [%fp+-24]
	call	BPF_FUNC_probe_read_str	/* r0 = bpf_probe_read_str(tmp1, r8, tmp1) */

	jsle	%r0, 0, .L0		/* help the BPF verifier */
	lddw	%r8, STRSZ
	jlt	%r0, %r8, 1
	mov	%r0, %r8

	sub	%r0, 1			/* r0 -= 1 */

	jgt	%r0, %r6, .Lsame	/* if (r0 > r6) goto Lsame */
	jgt	%r0, %r7, .Lsame	/* if (r0 > r7) goto Lsame */

	ldxdw	%r7, [%fp+-32]
	add	%r7, %r0
	ldxb	%r7, [%r7+0]
	and	%r7, 0xff		/* r7 = tmp2[r0] */

	ldxdw	%r1, [%fp+-24]
	mov	%r2, 1
	ldxdw	%r3, [%fp+-8]
	add	%r3, %r0
	call	BPF_FUNC_probe_read	/* bpf_probe_read(tmp1, 1, &s[r0]) */
	ldxdw	%r6, [%fp+-24]
	ldxb	%r6, [%r6+0]
	and	%r6, 0xff		/* r6 = tmp1[0] */

.Lsame:
	jle	%r6, %r7, 2		/* if (r6 > r7) return +1 */
	mov	%r0, 1
	exit
	jge	%r6, %r7, 2		/* if (r6 < r7) return -1 */
	mov	%r0, -1
	exit

.L0:
	mov	%r0, 0			/* return 0 */
	exit
	.size	dt_strcmp, .-dt_strcmp
