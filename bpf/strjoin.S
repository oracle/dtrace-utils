// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */

#define DT_STRLEN_BYTES		2

#define BPF_FUNC_probe_read_str	45

/*
 * void dt_strjoin(char *dst, const char *s1, const char *s2)
 */
	.text
	.align	4
	.global	dt_strjoin
	.type	dt_strjoin, @function
dt_strjoin:
	mov	%r9, %r1		/* %r9 = dst */
	mov	%r7, %r2		/* %r7 = s1 */
	mov	%r8, %r3		/* %r8 = s2 */

	lddw	%r6, STRSZ
	add	%r6, 1			/* cnt = STRSZ + 1 */

	add	%r1, DT_STRLEN_BYTES	/* dst is in %r1 already */
	mov	%r2, %r6
	mov	%r3, %r7
	add	%r3, DT_STRLEN_BYTES
	call	BPF_FUNC_probe_read_str	/*
					 * rc = probe_read_str(
					 *	  &dst[DT_STRLEN_BYTES],
					 *	  cnt,
					 *	  &s1[DT_STRLEN_BYTES]);
					 */
	jsle	%r0, 0, .Lexit		/* if (rc s<= 0) goto .Lexit; */
	mov	%r7, %r0		/* len = rc */
	jeq	%r7, %r6, .Lset_len	/* if (len == cnt) goto .Lset_len; */
	sub	%r7, 1			/* len-- */

	mov	%r1, %r9
	add	%r1, DT_STRLEN_BYTES
	add	%r1, %r7
	mov	%r2, %r6
	sub	%r2, %r7
	mov	%r3, %r8
	add	%r3, DT_STRLEN_BYTES
	call	BPF_FUNC_probe_read_str	/*
					 * rc = probe_read_str(
					 *	  &dst[DT_STRLEN_BYTES + len],
					 *	  cnt - len,
					 *	  &s2[DT_STRLEN_BYTES]);
					 */
	jsle	%r0, 0, .Lexit		/* if (rc s<= 0) goto .Lset_len; */
	add	%r7, %r0		/* len += rc */

.Lset_len:
	sub	%r7, 1			/* len-- */
	mov	%r1, %r7
	mov	%r2, %r9
	call	dt_strlen_store		/* dt_strlen_store(len - 1, dst) */

.Lexit:
	exit
	.size	dt_strjoin, .-dt_strjoin
