// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 */

	.text

/*
 * // This helper writes a uint8 as a string.
 * // The output buffer is dst[STRSZ] and we start writing at &dst[off].
 *
 * uint64_t dt_inet_ntoa_write_uint8(uint64_t inp, char *dst, uint64_t off) {
 *     uint64_t dig, nzr, len;
 *
 *     nzr = 0;         // nonzero if any nonzero digits have been written
 *     len = STRSZ;
 *     inp &= 0xff;
 *
 *     // write 100s digit
 *     if (off >= len) return off;
 *     dig = inp / 100;
 *     nzr += dig;
 *     dst[off] = '0' + dig;
 *     if (nzr != 0) off++;
 *
 *     // write 10s digit
 *     if (off >= len) return off;
 *     inp -= 100 * dig;
 *     dig = inp / 10;
 *     nzr += dig;
 *     dst[off] = '0' + dig;
 *     if (nzr != 0) off++;
 *
 *     // write 1s digit
 *     if (off >= len) return off;
 *     inp -= 10 * dig;
 *     dst[off] = '0' + inp;
 *     off++;
 *
 *     return off;
 * }
 *
 */

	.align	4
	.global	dt_inet_ntoa_write_uint8
	.type	dt_inet_ntoa_write_uint8, @function
dt_inet_ntoa_write_uint8:

#define OFF %r0
#define INP %r1
#define DST %r2
#define DIG %r5
#define NZR %r6
#define LEN %r7
#define CHR %r8
#define PTR %r9
	mov	OFF, %r3

	mov	NZR, 0
	lddw	LEN, STRSZ
	and	INP, 0xff

	/* write 100s digit */
	jlt	OFF, LEN, 1
	exit
	mov	DIG, INP
	div	DIG, 100
	add	NZR, DIG
	mov	CHR, DIG
	add	CHR, '0'
	mov	PTR, DST
	add	PTR, OFF
	stxb	[PTR+0], CHR
	jeq	NZR, 0, 1
	add	OFF, 1

	/* write 10s digit */
	jlt	OFF, LEN, 1
	exit
	mul	DIG, 100
	sub	INP, DIG
	mov	DIG, INP
	div	DIG, 10
	add	NZR, DIG
	mov	CHR, DIG
	add	CHR, '0'
	mov	PTR, DST
	add	PTR, OFF
	stxb	[PTR+0], CHR
	jeq	NZR, 0, 1
	add	OFF, 1

	/* write 1s digit */
	jlt	OFF, LEN, 1
	exit
	mul	DIG, 10
	sub	INP, DIG
	add	INP, '0'
	add	DST, OFF
	stxb	[DST+0], INP
	add	OFF, 1
	exit
	.size	dt_inet_ntoa_write_uint8, .-dt_inet_ntoa_write_uint8
#undef OFF
#undef INP
#undef DST
#undef DIG
#undef NZR
#undef LEN
#undef CHR
#undef PTR

/*
 * void dt_inet_ntoa(uint8_t *src, char *dst) {
 *     uint64_t off, inp, len;
 *
 *     off = 0
 *     len = STRSZ;
 *
 *     inp = src[0];
 *     off = dt_inet_ntoa_write_uint8(inp, dst, off);
 *     if (off >= STRSZ) goto done:
 *     dst[off++] = ':';
 *
 *     inp = src[1];
 *     off = dt_inet_ntoa_write_uint8(inp, dst, off);
 *     if (off >= STRSZ) goto done:
 *     dst[off++] = ':';
 *
 *     inp = src[2];
 *     off = dt_inet_ntoa_write_uint8(inp, dst, off);
 *     if (off >= STRSZ) goto done:
 *     dst[off++] = ':';
 *
 *     inp = src[3];
 *     off = dt_inet_ntoa_write_uint8(inp, dst, off);
 *
 *     done:
 *     dst[off] = '\0';
 * }
 *
 */
	.align	4
	.global	dt_inet_ntoa
	.type	dt_inet_ntoa, @function
dt_inet_ntoa:

/* off cycles between %r3 (input arg to subroutine) and %r0 (its return) */
#define INP %r1
#define SRC %r6
#define DST %r7
#define LEN %r8

	mov	SRC, %r1
	mov	DST, %r2

	mov	%r3, 0
	lddw	LEN, STRSZ

	ldxb	INP, [SRC+0]
	mov	%r2, DST
	call	dt_inet_ntoa_write_uint8
	jge	%r0, LEN, .Ldone
	mov	%r3, %r0
	add	%r3, 1
	add	%r0, DST
	stb	[%r0+0], '.'

	ldxb	INP, [SRC+1]
	mov	%r2, DST
	call	dt_inet_ntoa_write_uint8
	jge	%r0, LEN, .Ldone
	mov	%r3, %r0
	add	%r3, 1
	add	%r0, DST
	stb	[%r0+0], '.'

	ldxb	INP, [SRC+2]
	mov	%r2, DST
	call	dt_inet_ntoa_write_uint8
	jge	%r0, LEN, .Ldone
	mov	%r3, %r0
	add	%r3, 1
	add	%r0, DST
	stb	[%r0+0], '.'

	ldxb	INP, [SRC+3]
	mov	%r2, DST
	call	dt_inet_ntoa_write_uint8

.Ldone:
	add	DST, %r0
	stb	[DST+0], 0
	exit
	.size	dt_inet_ntoa, .-dt_inet_ntoa
#undef INP
#undef SRC
#undef DST
#undef LEN
