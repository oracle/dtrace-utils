// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2023, Oracle and/or its affiliates. All rights reserved.
 */

#define BPF_FUNC_probe_read	4
#define MAXCHARS		19

	.text
/*
 * void dt_lltostr(const dt_dctx_t *dctx, uint64_t VAL, char *STR)
 * {
 *     // start writing at the end (IDX==1 is the last char)
 *     IDX = 1
 *
 *     // compute sign bit
 *     SGN = VAL >> 63
 *
 *     // replace VAL with its absolute value
 *     VAL *= (1 - 2 * SGN)
 *
 *     // prewrite a '0' in case VAL==0
 *     [%fp + -1] = '0'
 *
 *     Lloop:
 *         if (VAL == 0) goto Ldone
 *
 *         r5 = VAL / 10
 *         r3 = itoa(VAL % 10)
 *
 *         [%fp + -IDX] = r3
 *
 *         VAL = r5
 *         IDX++
 *
 *         jump Lloop
 *
 *     Ldone:
 *
 *     // back IDX up one char (unless no chars were written in loop)
 *     if (IDX > 1) IDX--
 *
 *     // if (SGN), account for sign char
 *     IDX += SGN
 *
 *     LEN = min(IDX, STRSZ)
 *
 *     // copy to output string
 *     bpf_probe_read(STR, LEN, %fp + -IDX)
 *
 *     // if (SGN), we included a garbage sign char; fix it
 *     if (SGN) STR[0] = '-'
 *
 *     // add terminating NULL byte
 *     if (LEN < STRSZ) STR[LEN] = '\0'
 * }
 */
	.align	4
	.global	dt_lltostr
dt_lltostr:

/*
 * assign certain variables to registers:
 *   - STR is the output string
 *   - VAL is the value we are trying to convert
 *   - IDX is the index of the output char we are writing
 *       (1-based, starting from the end)
 *   - SGN is the sign bit of the input value (1: neg; 0: otherwise)
 * registers r0-r5 are used as temporary variables
 *   (including of course for function calls)
 */
#define STR %r6
#define VAL %r7
#define IDX %r8
#define SGN %r9

	mov	VAL, %r2
	mov	STR, %r3

	mov	IDX, 1				/* IDX = 1 */

	mov	SGN, VAL
	rsh	SGN, 63				/* SGN = VAL >> 63 */

	mov	%r5, 1
	sub	%r5, SGN
	sub	%r5, SGN
	mul	VAL, %r5			/* VAL *= (1 - 2 * SGN) */

	mov	%r5, '0'
	stxb	[%fp+-1], %r5			/* prewrite a '0' in case VAL==0 */

.Lloop:
	jeq	VAL, 0, .Ldone			/* if (VAL == 0) goto Ldone */
	jgt	IDX, MAXCHARS, .Ldone		/* help the BPF verifier */

	mov	%r5, VAL
	div	%r5, 10				/* r5 = VAL / 10 */

	mov	%r4, 10
	mul	%r4, %r5
	mov	%r3, VAL
	sub	%r3, %r4
	add	%r3, '0'			/* r3 = itoa(VAL % 10) */

	mov	%r2, %fp
	mov	%r1, IDX
	mul	%r1, -1
	add	%r2, %r1
	stxb	[%r2+0], %r3			/* [%fp + -IDX] = r3 */

	mov	VAL, %r5			/* VAL = r5 */
	add	IDX, 1				/* IDX++ */

	ja	.Lloop

.Ldone:

/* at this point VAL==0, so use its register for string length */
#undef VAL
#define LEN %r7

	jle	IDX, 1, 1			/* if (IDX > 1) IDX-- */
	sub	IDX, 1

	jge	IDX, 1, 1			/* help the BPF verifier out */
	mov	IDX, 1
	jle	IDX, MAXCHARS, 1
	mov	IDX, MAXCHARS
	and	SGN, 1

	add	IDX, SGN			/* IDX += SGN */

	lddw	%r1, STRSZ
	mov	LEN, IDX
	jle	LEN, %r1, 1
	mov	LEN, %r1			/* LEN = min(IDX, STRSZ) */

	mov	%r1, STR
	mov	%r2, LEN
	mov	%r3, %fp
	mov	%r4, 0
	sub	%r4, IDX
	add	%r3, %r4
	call	BPF_FUNC_probe_read		/* bpf_probe_read(STR, LEN, %fp + -IDX) */

	mov	%r5, '-'
	jeq	SGN, 0, 1
	stxb	[STR+0], %r5			/* if (SGN) STR[0] = '-' */

	lddw	%r5, STRSZ
	jlt	LEN, %r5, 1
	exit
	add	STR, LEN
	mov	%r5, 0
	stxb	[STR+0], %r5			/* if (LEN < STRSZ) STR[LEN] = '\0' */
	exit
