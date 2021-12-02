// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */

#define DT_STRLEN_BYTES		2

#define BPF_FUNC_probe_read_str	45

/*
 * void dt_basename(char *src, char *dst);
 */
	.text
	.align	4
	.global	dt_basename
	.type	dt_basename, @function
dt_basename :
#define SRC %r6
#define DST %r7
#define BGN %r8
#define LEN %r9

	/* store copies of input arguments */
	mov	SRC, %r1
	mov	DST, %r2

	/* ignore the string-length prefix */
	add	SRC, DT_STRLEN_BYTES

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
	 * Loop over len, backing it up to find a non-'/' char.
	 */
.Lend:
	/* len-- */
	sub	LEN, 1
	/* if (len s< 0) goto Lslash */
	jslt	LEN, 0, .Lslash
	/* if (src[len] == '/') goto Lend */
	mov	%r1, SRC
	add	%r1, LEN
	ldxb	%r1, [%r1+0]
	and	%r1, 0xff
	jeq	%r1, '/', .Lend

	/*
	 * Loop over bgn, backing it up to find a '/' char.
	 */
	/* bgn = len */
	mov	BGN, LEN
.Lbgn:
	/* bgn-- */
	sub	BGN, 1
	/* if (bgn s< 0) goto Lcopy */
	jslt	BGN, 0, .Lcopy
	/* if (src[bgn] != '/') goto Lbgn */
	mov	%r1, SRC
	add	%r1, BGN
	ldxb	%r1, [%r1+0]
	and	%r1, 0xff
	jne	%r1, '/', .Lbgn

.Lcopy:
	/*
	 * The output string is a copy of the designated substring.
	 */
	/* len -= bgn (and help the BPF verifier) */
	sub	LEN, BGN
	jsge	LEN, 0, 1
	mov	LEN, 0
	/* bgn++ */
	add	BGN, 1
	/* bpf_probe_read_str(dst + DT_STRLEN_BYTES, len + 1, &src[bgn]) */
	mov	%r1, DST
	add	%r1, DT_STRLEN_BYTES
	mov	%r2, LEN
	add	%r2, 1
	mov	%r3, SRC
	add	%r3, BGN
	call	BPF_FUNC_probe_read_str

.Lcoda:
	/* dt_strlen_store(len, dst) */
	mov	%r1, LEN
	mov	%r2, DST
	call	dt_strlen_store

	/* return */
	exit

.Ldot:
	/*
	 * The output string is simply ".\0".
	 */
	mov	LEN, 1
	stb	[DST+DT_STRLEN_BYTES], '.'
	stb	[DST+(DT_STRLEN_BYTES+1)], 0
	ja	.Lcoda

.Lslash:
	/*
	 * The output string is simply "/\0".
	 */
	mov	LEN, 1
	stb	[DST+DT_STRLEN_BYTES], '/'
	stb	[DST+(DT_STRLEN_BYTES+1)], 0
	ja	.Lcoda
#undef SRC
#undef DST
#undef BGN
#undef LEN
	.size	dt_basename, .-dt_basename
