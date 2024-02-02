// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>

/*
 * void dt_dirname(const dt_dctx_t *dctx, char *src, char *dst);
 */
	.text
	.align	4
	.global	dt_dirname
	.type	dt_dirname, @function
dt_dirname :
#define SRC %r6
#define DST %r7
#define LEN %r8

	/* store copies of input arguments */
	mov	SRC, %r2
	mov	DST, %r3

	/*
	 * Copy src to dst for two reasons:
	 *   - to get the string length
	 *   - to have a copy that we can access directly
	 */
	/* r0 = bpf_probe_read_str(dst, STRSZ + 1, src) */
	mov	%r1, DST
	lddw	%r2, STRSZ
	add	%r2, 1
	mov	%r3, SRC
	call	BPF_FUNC_probe_read_str

	/* if (r0 s<= 1) goto Ldot */
	jsle	%r0, 1, .Ldot

	/* len = r0 - 1 */
	mov	LEN, %r0
	sub	LEN, 1

	/*
	 * In general, the string will look like
	 * "dirname////basename////".
	 */

	/*
	 * Back up, looking for a non-'/' char.
	 * That gives us the end of basename.
	 */
.L1:
	/* len-- */
	sub	LEN, 1
	/* if (len s< 0) goto Lslash */
	jslt	LEN, 0, .Lslash
	/* if (dst[len] == '/') goto L1 */
	mov	%r0, DST
	add	%r0, LEN
	ldxb	%r0, [%r0+0]
	and	%r0, 0xff
	jeq	%r0, '/', .L1

	/*
	 * Back up, looking for a '/' char.
	 * That gives us right before the start of basename.
	 */
.L2:
	/* len-- */
	sub	LEN, 1
	/* if (len s< 0) goto Ldot */
	jslt	LEN, 0, .Ldot
	/* if (dst[len] != '/') goto L2 */
	mov	%r0, DST
	add	%r0, LEN
	ldxb	%r0, [%r0+0]
	and	%r0, 0xff
	jne	%r0, '/', .L2

	/*
	 * Back up, looking for a non-'/' char.
	 * That gives us the end of dirname.
	 */
.L3:
	/* len-- */
	sub	LEN, 1
	/* if (len s< 0) goto Lslash */
	jslt	LEN, 0, .Lslash
	/* if (dst[len] == '/') goto L3 */
	mov	%r0, DST
	add	%r0, LEN
	ldxb	%r0, [%r0+0]
	and	%r0, 0xff
	jeq	%r0, '/', .L3

	/* len++ */
	add	LEN, 1

	/*
	 * The output string is up to the end of the dirname.
	 */

	/* bpf_probe_read_str(dst, len + 1, src) */
	mov	%r1, DST
	mov	%r2, LEN
	add	%r2, 1
	mov	%r3, SRC
	call	BPF_FUNC_probe_read_str

	/* return */
	exit

.Ldot:
	/*
	 * The output string is simply ".".
	 */
	mov	LEN, 1
	stb	[DST+0], '.'
	stb	[DST+1], 0
	exit

.Lslash:
	/*
	 * The output string is simply "/".
	 */
	mov	LEN, 1
	stb	[DST+0], '/'
	stb	[DST+1], 0
	exit
#undef SRC
#undef DST
#undef LEN
	.size	dt_dirname, .-dt_dirname
