// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */

#define DT_STRLEN_BYTES		2

#define BPF_FUNC_probe_read	4

/*
 * void dt_substr(char *dst, const char *src, int32_t idx, int32_t cnt,
 *		  uint64_t argc)
 */
	.text
	.align	4
	.global	dt_substr
	.type	dt_substr, @function
dt_substr :
	mov	%r9, %r1		/* %r9 = dst */
	stxdw	[%fp+-8], %r2		/* Spill src */
	lsh	%r3, 32			/* Sign-extend idx */
	arsh	%r3, 32
	mov	%r6, %r3		/* %r6 = idx */

	lddw	%r8, STRSZ		/* %r8 = STRSZ (temporary) */
	jgt	%r5, 2, .Lhave_cnt
	mov	%r4, %r8		/* cnt = STRSZ */
.Lhave_cnt:
	lsh	%r4, 32			/* Sign-extend cnt */
	arsh	%r4, 32
	mov	%r7, %r4		/* %r7 = cnt */

	/*
	 * Get the source string length and validate it.  If the length is 0,
	 * the result is the empty string.  If the length is greater than the
	 * maximum string length (STRSZ), cap it at that value.
	 */
	ldxdw	%r1, [%fp+-8]
	call	dt_strlen		/* len = dt_strlen(src) */
	jeq	%r0, 0, .Lempty
	mov	%r1, %r8		/* %r1 = STRSZ */
	jle	%r0, %r8, .Llen_ok
	mov	%r0, %r8		/* len = STRSZ */
.Llen_ok:
	mov	%r8, %r0		/* %r8 = len */

	jsge	%r6, 0, .Ladjust_cnt

	add	%r6, %r8		/* idx += len */
	jsge	%r6, 0, .Ladjust_cnt
	mov	%r0, 0
	sub32	%r0, %r6		/* neg messes up the verifier */
	jsle	%r7, %r0, .Ladjust_cnt
	add32	%r7, %r6		/* cnt += idx */
	mov	%r6, 0			/* idx = 0 */

.Ladjust_cnt:
	jsge	%r6, %r1, .Lempty
	jsge	%r6, %r8, .Lempty
	jslt	%r6, 0, .Lempty
	jsge	%r7, 0, .Lcnt_pos
	mov	%r0, %r8
	sub32	%r0, %r6
	add32	%r7, %r0		/* cnt += len - idx */
	lsh	%r7, 32
	arsh	%r7, 32
	jsle	%r7, 0, .Lempty
.Lcnt_pos:
	sub	%r1, %r6
	jsge	%r1, %r7, .Lcopy
	mov	%r7, %r1		/* cnt = STRSZ - idx */

.Lcopy:
	ldxdw	%r8, [%fp+-8]
	add	%r8, DT_STRLEN_BYTES
	add	%r8, %r6		/* %r8 = src + DT_STRLEN_BYTES + idx */

	mov	%r1, %r7
	mov	%r2, %r9
	call	dt_strlen_store		/* dt_strlen_store(cnt, dst) */
	add	%r9, DT_STRLEN_BYTES	/* %r9 = dst + DT_STRLEN_BYTES */

	mov	%r1, %r9
	mov	%r2, %r7
	mov	%r3, %r8
	call	BPF_FUNC_probe_read

	add	%r9, %r7
	stb	[%r9+0], 0
	exit

.Lempty:
	/* Store the empty string in the destination. */
	stb	[%r9+0], 0
	stb	[%r9+1], 0
	stb	[%r9+2], 0
	exit
	.size	dt_substr, .-dt_substr
