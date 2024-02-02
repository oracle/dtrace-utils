// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>

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

	mov	%r2, %r6
	mov	%r3, %r7
	call	BPF_FUNC_probe_read_str	/* rc = probe_read_str(dst, cnt, s1); */
	jsle	%r0, 0, .Lexit		/* if (rc s<= 0) goto .Lexit; */
	mov	%r7, %r0		/* len = rc */
	jeq	%r7, %r6, .Lexit	/* if (len == cnt) goto .Lexit; */
	sub	%r7, 1			/* len-- */

	mov	%r1, %r9
	add	%r1, %r7
	mov	%r2, %r6
	sub	%r2, %r7
	mov	%r3, %r8
	call	BPF_FUNC_probe_read_str	/*
					 * rc = probe_read_str(dst, cnt - len,
					 *		       s2);
					 */
	jsle	%r0, 0, .Lexit		/* if (rc s<= 0) goto .Lexit */
	add	%r7, %r0		/* len += rc */

.Lexit:
	exit
	.size	dt_strjoin, .-dt_strjoin
