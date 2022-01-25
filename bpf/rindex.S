// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * This approach to implementing rindex(s, t[, start]) mimics the approach
 * used for index().  Notably, it uses dt_index_match().
 *
 * The rindex() semantics are to find the highest index where t appears in s,
 * but at most the start value.
 *
 * The user semantics are that if the start is not specified, it is as though
 * len(s)-len(t) were used.  A negative start means no match will be found.
 * These user semantics are modified internally, within the implementation:
 *   - In dt_cg.c, start<0 is immediately rejected.
 *   - In the call to rindex.S, start<0 means to use the default.
 */

#define BPF_FUNC_probe_read	4
#define BPF_FUNC_probe_read_str	45

	.text
/*
 * int dt_rindex(const char *s, const char *t, int start, char *tmp1, char *tmp2)
 * {
 *     uint64_t r0, tlen;
 *     uint64_t buflen;
 *
 *     // round buflen for dt_index_match()
 *     buflen = STRSZ rounded up to multiple of 8;
 *
 *     // keep a copy of t in tmp2
 *     tlen = bpf_probe_read_str(tmp2, buflen, t);
 *
 *     // determine actual start index
 *     r0 = bpf_probe_read_str(tmp1, buflen, s);
 *     r0 -= tlen;     // maximum possible index value
 *     if (start < 0) start = r0;
 *     if (start > r0) start = r0;
 *
 *     // drop terminating NULL
 *     tlen--;
 *
 *     // Fill end of tmp1 with contents from tmp2
 *     // to suppress spurious mismatches.
 *     bpf_probe_read(tmp1 + tlen, buflen - tlen, tmp2 + tlen);
 *
 *     // Apparently, the BPF verifier prefers incrementing loop counts.
 *     // So loop cnt goes from 0 to start; idx goes from start to 0.
 *     cnt = 0;
 *     Lloop:
 *         // check loop
 *         if (cnt > start) return -1;
 *         idx = start - cnt;
 *
 *         // fill start of tmp1 with s, starting at the proposed index
 *         bpf_probe_read(tmp1, tlen, s + idx);
 *
 *         // keep looping if not a match
 *         r0 = dt_index_match(tmp1, tmp2, buflen);
 *         cnt++;
 *         if (r0 == 0) goto Lloop;
 *
 *      return idx;
 * }
 *
 * Some variables are kept in registers or spilled to the stack:
 *     r6 = idx          [%fp+-8] = s
 *     r7 = tmp1         [%fp+-16] = buflen
 *     r8 = tmp2         [%fp+-24] = start
 *     r9 = tlen         [%fp+-32] = cnt
 * but t is not needed once we have copied its contents to tmp2.
 */
	.align	4
	.global	dt_rindex
	.type	dt_rindex, @function
dt_rindex:
	lddw	%r6, STRSZ
	add	%r6, 7
	and	%r6, -8
	stxdw	[%fp+-16], %r6			/* buflen = STRSZ rounded up to multiple of 8 */

	stxdw	[%fp+-8], %r1			/* stash copies of some variables */
	stxdw	[%fp+-24], %r3
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
	call	BPF_FUNC_probe_read_str		/* r0 = bpf_probe_read_str(tmp1, buflen, s) */

	sub	%r0, %r9			/* r0 -= tlen */
	jslt	%r0, 0, .Lerror
	ldxdw	%r3, [%fp+-24]
	jsge	%r3, 0, 1			/* if (start < 0) start = r0 */
	mov	%r3, %r0
	jle	%r3, %r0, 1			/* if (start > r0) start = r0 */
	mov	%r3, %r0
	stxdw	[%fp+-24], %r3

	sub	%r9, 1				/* tlen-- */

	mov	%r1, %r7
	add	%r1, %r9
	ldxdw	%r2, [%fp+-16]
	sub	%r2, %r9
	mov	%r3, %r8
	add	%r3, %r9
	call	BPF_FUNC_probe_read		/* bpf_probe_read(tmp1 + tlen, buflen - tlen, tmp2 + tlen) */

	mov	%r0, 0
	stxdw	[%fp+-32], %r0			/* cnt = 0 */
.Lloop:
	ldxdw	%r0, [%fp+-32]
	ldxdw	%r6, [%fp+-24]
	jgt	%r0, %r6, .Lerror		/* if (cnt > start) return -1 */

	/* help the BPF verifier */
	ldxdw	%r2, [%fp+-16]
	jge	%r0, %r2, .Lerror		/* if (cnt >= buflen) return -1 */

	sub	%r6, %r0			/* idx = start - cnt */
	jlt	%r6, 0, .Lerror
	jge	%r6, %r2, .Lerror

	mov	%r1, %r7
	mov	%r2, %r9
	ldxdw	%r3, [%fp+-8]
	add	%r3, %r6
	call	BPF_FUNC_probe_read		/* bpf_probe_read(tmp1, tlen, s + idx) */

	mov	%r1, %r7
	mov	%r2, %r8
	ldxdw	%r3, [%fp+-16]
	call	dt_index_match			/* r0 = dt_index_match(tmp1, tmp2, buflen) */

	ldxdw	%r1, [%fp+-32]
	add	%r1, 1				/* cnt++ */
	stxdw	[%fp+-32], %r1

	jeq	%r0, 0, .Lloop			/* if (r0 == 0) goto Lloop */

	/* done */
	mov	%r0, %r6			/* return idx */
	exit

.Lerror:
	mov	%r0, -1
	exit
	.size	dt_rindex, .-dt_rindex
