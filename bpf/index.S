// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 */

#define BPF_FUNC_probe_read	4
#define BPF_FUNC_probe_read_str	45

	.text
/*
 * For two buffers, return:
 *     1 if the two buffers match in every bit
 *     0 if the two buffers mismatch in any bit
 * To help the BPF verifier, minimize branching by:
 *   - using a len whose exact value is known to the BPF verifier
 *   - operating on 64 bits at a time (len is a multiple of 8)
 *   - replacing conditional branching with arithmetic operations
 *       such as ^ | & etc.
 *
 * uint64_t dt_index_match(char *tmp1, char *tmp2, uint64_t len)
 * {
 *     r0 = 0;          // accumulate bits indicating mismatches
 *     r6 = 0;          // loop counter
 *     L1:
 *         if (r6 >= len) goto L2;
 *
 *         r4 = *((uint64_t*)&tmp1[r6]);
 *         r5 = *((uint64_t*)&tmp2[r6]);
 *         r0 |= (r4 ^ r5);
 *
 *         r6 += 8;
 *         goto L1;
 *
 *     L2:
 *                       //            value of r0
 *                       //   perfect match    any mismatches
 *                       //        == 0              != 0
 *     r0 |= (r0 >> 32); //        == 0              != 0
 *     r0 &= 0xffffffff; //        == 0              >  0
 *     r0 -= 1;          //        <  0              >= 0
 *     r0 >>= 63;        //        == 1              == 0
 *
 *     return r0;
 * }
 */
	.align	4
	.global	dt_index_match
	.type	dt_index_match, @function
dt_index_match:
	mov	%r0, 0
	mov	%r6, 0

.L1:
	jge	%r6, %r3, .L2

	mov	%r4, %r1
	add	%r4, %r6
	ldxdw	%r4, [%r4+0]
	mov	%r5, %r2
	add	%r5, %r6
	ldxdw	%r5, [%r5+0]
	xor	%r4, %r5
	or	%r0, %r4

	add	%r6, 8
	ja	.L1

.L2:
	mov	%r4, %r0
	rsh	%r4, 32
	or	%r0, %r4
	and	%r0, 0xffffffff
	sub	%r0, 1
	rsh	%r0, 63

	exit
	.size	dt_index_match, .-dt_index_match

/*
 * int dt_index(const char *s, const char *t, int start, char *tmp1, char *tmp2)
 * {
 *     uint64_t r0, tlen;
 *     uint64_t buflen;
 *
 *     // determine actual start index
 *     if (start < 0) start = 0;
 *
 *     // round buflen for dt_index_match()
 *     buflen = STRSZ rounded up to multiple of 8;
 *
 *     // keep a copy of t in tmp2
 *     tlen = bpf_probe_read_str(tmp2, buflen, t);
 *
 *     // determine maximum possible index value
 *     maxi = bpf_probe_read_str(tmp1, buflen, s);
 *     maxi -= tlen;
 *
 *     // drop terminating NULL
 *     tlen--;
 *
 *     // Fill end of tmp1 with contents from tmp2
 *     // to suppress spurious mismatches.
 *     bpf_probe_read(tmp1 + tlen, buflen - tlen, tmp2 + tlen);
 *
 *     Lloop:
 *         // check loop
 *         if (start > maxi) return -1;
 *
 *         // fill start of tmp1 with s, starting at the proposed index
 *         bpf_probe_read(tmp1, tlen, s + start);
 *
 *         // keep looping if not a match
 *         r0 = dt_index_match(tmp1, tmp2, buflen);
 *         start++;
 *         if (r0 == 0) goto Lloop;
 *
 *      start--;
 *      return start;
 * }
 *
 * Some variables are kept in registers or spilled to the stack:
 *     r6 = start        [%fp+-8] = s
 *     r7 = tmp1         [%fp+-16] = buflen
 *     r8 = tmp2         [%fp+-24] = maxi
 *     r9 = tlen
 * but t is not needed once we have copied its contents to tmp2.
 */
	.align	4
	.global	dt_index
	.type	dt_index, @function
dt_index:
	jsge	%r3, 0, 1
	mov	%r3, 0				/* if (start < 0) start = 0 */

	lddw	%r6, STRSZ
	add	%r6, 7
	and	%r6, -8
	stxdw	[%fp+-16], %r6			/* buflen = STRSZ rounded up to multiple of 8 */

	stxdw	[%fp+-8], %r1			/* stash copies of some variables */
	mov	%r6, %r3
	mov	%r7, %r4
	mov	%r8, %r5

	mov	%r3, %r2
	ldxdw	%r2, [%fp+-16]
	mov	%r1, %r8
	call	BPF_FUNC_probe_read_str		/* tlen = bpf_probe_read_str(tmp2, buflen, t) */
	jsle	%r0, 0, .Lerror
	mov	%r9, %r0

	mov	%r1, %r7
	ldxdw	%r2, [%fp+-16]
	ldxdw	%r3, [%fp+-8]
	call	BPF_FUNC_probe_read_str		/* maxi = bpf_probe_read_str(tmp1, buflen, s) */

	sub	%r0, %r9			/* maxi -= tlen */
	jslt	%r0, 0, .Lerror
	stxdw	[%fp+-24], %r0

	sub	%r9, 1				/* tlen-- */

	mov	%r1, %r7
	add	%r1, %r9
	ldxdw	%r2, [%fp+-16]
	sub	%r2, %r9
	mov	%r3, %r8
	add	%r3, %r9
	call	BPF_FUNC_probe_read		/* bpf_probe_read(tmp1 + tlen, buflen - tlen, tmp2 + tlen) */

.Lloop:
	/* help the BPF verifier */
	ldxdw	%r0, [%fp+-16]
	jge	%r6, %r0, .Lerror		/* if (start >= buflen) goto Lerror */

	ldxdw	%r0, [%fp+-24]
	jgt	%r6, %r0, .Lerror		/* if (start > maxi) goto Lerror */

	mov	%r1, %r7
	mov	%r2, %r9
	ldxdw	%r3, [%fp+-8]
	add	%r3, %r6
	call	BPF_FUNC_probe_read		/* bpf_probe_read(tmp1, tlen, s + start) */

	mov	%r1, %r7
	mov	%r2, %r8
	ldxdw	%r3, [%fp+-16]
	call	dt_index_match			/* r0 = dt_index_match(tmp1, tmp2, buflen) */

	add	%r6, 1				/* start++ */
	jeq	%r0, 0, .Lloop			/* if (r0 == 0) goto Lloop */

	/* done */
	sub	%r6, 1				/* start-- */
	mov	%r0, %r6			/* return start */
	exit

.Lerror:
	mov	%r0, -1
	exit
	.size	dt_index, .-dt_index
