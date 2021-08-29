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

	mov	%r1, %r7
	call	dt_strlen
	mov	%r6, %r0		/* len = dt_strlen(s1) */
	add	%r7, DT_STRLEN_BYTES	/* s1 += 2 */

	mov	%r1, %r8
	call	dt_strlen
	add	%r6, %r0		/* len += dt_strlen(s2) */
	add	%r8, DT_STRLEN_BYTES	/* s2 += 2 */

	mov	%r1, %r6
	mov	%r2, %r9
	call	dt_strlen_store		/* dt_strlen_store(len, dst) */
	add	%r9, DT_STRLEN_BYTES	/* dst += 2 */

	lddw	%r6, STRSZ
	add	%r6, 1
	and	%r6, 0xffffffff		/* len = (STRSZ + 1) & 0xffffffff */

	mov	%r1, %r9
	mov	%r2, %r6
	mov	%r3, %r7
	call	BPF_FUNC_probe_read_str	/* cnt = bpf_probe_read_str(dst, len s1) */
	jslt	%r0, 0, .L1		/* if (cnt < 0) goto .L1 */
	jslt	%r0, 1, .L2		/* if (cnt < 1) goto .L2 */
	sub	%r0, 1			/* cnt-- */
	add	%r9, %r0		/* dst += cnt */
	sub	%r6, %r0		/* len -= cnt */
.L2:
	mov	%r1, %r9
	mov	%r2, %r6
	mov	%r3, %r8
	call	BPF_FUNC_probe_read_str	/* bpf_probe_read_str(dst, len, s2 */
.L1:
	exit
	.size	dt_strjoin, .-dt_strjoin
