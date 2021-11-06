// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */

#define DT_STRLEN_BYTES		2

#define BPF_FUNC_probe_read_str	45

/*
 * void dt_substr(char *dst, const char *src, int32_t idx, int32_t cnt,
 *		  uint64_t argc)
 *
 * %r1 = dst, %r2 = src, %r3 = idx, %r4 = cnt, %r5 = argc
 */
	.text
	.align	4
	.global	dt_substr
	.type	dt_substr, @function
dt_substr :
	/* Store the arguments (sign-extend idx). */
	mov	%r9, %r1		/* %r9 = dst */
	stxdw	[%fp+-8], %r2		/* Spill src */
	lsh	%r3, 32			/* Sign-extend idx */
	arsh	%r3, 32
	mov	%r6, %r3		/* %r6 = idx */

	/* Initialize the string length with its maximum value.  */
	lddw	%r8, STRSZ		/* %r8 = len = STRSZ */

	/*
	 * If we do not have a cnt, use the maximum value.
	 * Otherwise, sign-extend cnt.
	 */
	jgt	%r5, 2, .Lhave_cnt	/* if (argc > 2) goto Lhave_cnt; */
	mov	%r7, %r8		/* cnt = STRSZ */
	ja	.Lcnt_set
.Lhave_cnt:
	lsh	%r4, 32			/* Sign-extend cnt */
	arsh	%r4, 32
	mov	%r7, %r4		/* %r7 = cnt */
.Lcnt_set:

	/*
	 * Get the source string length and validate it.
	 * If the length is 0, the result is the empty string.
	 * If the length is less than the maximum string length (STRSZ), use
	 * the length we got.  (The length is initialized in %r8 as the default
	 * string length.)
	 */
	ldxdw	%r1, [%fp+-8]
	call	dt_strlen		/* len = dt_strlen(src) */
	jeq	%r0, 0, .Lempty		/* if (len == 0) goto Lempty; */
	jge	%r0, %r8, .Llen_set	/* if (len >= STRSZ) goto Llen_set; */
	mov	%r8, %r0		/* %r8 = len */
.Llen_set:

	jsge	%r6, 0, .Lcheck_idx	/* if (idx s>= 0) goto Lcheck_idx; */

	/*
	 * If idx is negative it is a count from the end of the string.  Turn
	 * it into the equivalent character offset (len + idx).
	 * If the offset is not negative, we can use it as the index.
	 * If not, we need to determine whether idx + cnt falls within the
	 * [0, len[ interval.  If it does not, the result is the empty string.
	 * If it does, we will copy cnt + idx characters, starting from idx 0..
	 */
	add	%r6, %r8		/* idx += len */
	jsge	%r6, 0, .Lcheck_idx	/* if (idx s>= 0) goto Lcheck_idx; */
	mov	%r0, 0
	sub	%r0, %r6		/* neg messes up the verifier */
	jsle	%r7, %r0, .Lempty	/* if (cnt s<= -idx) goto Lempty; */
	add	%r7, %r6		/* cnt += idx */
	mov	%r6, 0			/* idx = 0 */

.Lcheck_idx:
	/*
	 * Validate the idx value.  If idx is greater than the string length,
	 * we get the empty string.
	 */
	jsge	%r6, %r8, .Lempty	/* if (idx s>= len) goto Lempty; */

	/* If cnt is positive (or 0), we are ready to copy the slice. */
	jsge	%r7, 0, .Lcopy		/* if (cnt s>= 0) goto Lcopy; */

	/*
	 * If cnt is negative it is a count from the last character in string.
	 * Use the equivalent character offset to calculate the true (positive)
	 * cnt:
	 *	start = idx
	 *	end = (len - 1) - (-cnt)
	 *	new_cnt = end - start + 1
	 *		= (len - 1) - (-cnt) - idx + 1
	 *		= len - 1 + cnt - idx + 1
	 *		= len + cnt - idx
	 * which is equivalent with:
	 *	cnt += len - idx
	 */
	mov	%r0, %r8
	sub	%r0, %r6
	add	%r7, %r0		/* cnt += len - idx */
	jsle	%r7, 0, .Lempty		/* if (cnt s<= 0) goto Lempty; */

.Lcopy:
	/*
	 * Use the probe_read_str() BPF helper to copy (cnt + 1) bytes from
	 * &src[DT_STRLEN_BYTES + idx] to &dst[DT_STRLEN_BYTES].  We ensure
	 * that cnt is capped at STRSZ.
	 */
	mov	%r1, %r9
	add	%r1, DT_STRLEN_BYTES
	mov	%r2, %r7
	lddw	%r0, STRSZ
	jle	%r2, %r0, .Lcnt_ok	/* if (cnt <= STRSZ) goto Lcnt_ok; */
	mov	%r2, %r0		/* cnt = STRSZ */
.Lcnt_ok:
	add	%r2, 1
	ldxdw	%r3, [%fp+-8]
	add	%r3, DT_STRLEN_BYTES
	add	%r3, %r6
	call	BPF_FUNC_probe_read_str	/*
					 * rc = probe_read_str(
					 *	&dst[DT_STRLEN_BYTES],
					 *	cnt + 1,
					 *	&src[DT_STRLEN_BYTES + idx]);
					 */

	/* Store the result string length (rc - 1) at dst. */
	mov	%r1, %r0
	sub	%r1, 1
	mov	%r2, %r9
	call	dt_strlen_store		/* dt_strlen_store(rc - 1, dst) */

	exit

.Lempty:
	/* Store the empty string in the destination. */
	stb	[%r9+0], 0
	stb	[%r9+1], 0
	stb	[%r9+2], 0
	exit
	.size	dt_substr, .-dt_substr
