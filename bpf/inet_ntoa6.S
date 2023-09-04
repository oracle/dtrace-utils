// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 */

#define BPF_FUNC_probe_read		4

/*
 * We sadly cannot include struct declarations in assembler input files, so we
 * cannot use offsetof() to programmatically determine the offset of the rodata
 * member in the DTrace context (dt_ctx_t).  If that structure changes, the
 * following define must be updated as well.
 */
#define DCTX_RODATA			56

#define OUTPUT_LEN			40
#define INPUT_LEN			16

	.text

/*
 * uint64_t write_hex16(const uint16_t src, char *dst)
 */
	.align	4
	.global	write_hex16
	.type	write_hex16, @function
write_hex16:
	mov	%r0, 0

	/*
	 * Branch-free implementation of num-to-hex print function.  Given a
	 * number from 0 to 15, this will output a hex digit (0-9a-f) in the
	 * output buffer.  It also supports suppression of leading 0s if it is
	 * used to output a sequence of digits.
	 *
	 * Given: c (%r3) in [0, 15]
	 * Then, (c - 9) > 0 for c in [10, 15].
	 * Therefore, (-(c - 9)) has its highest bit set iff c in [10, 15].
	 * Thus, ((-(c - 9)) >> 63) is 1 iff c in [10, 15], and otherwise 0.
	 * Therefore, the hex digit (character) representing c can be computed
	 * as:
	 *	c + '0' + ((-(c - 9)) >> 63) * ('a' - '0' - 10)
	 *
	 * Let s (%r0) be the number of digits output thus far.  It should be
	 * incremented if it is non-zero or if the current digit is non-zero,
	 * which can be expressed as (s + c) > 0.  We only advance the output
	 * pointer if (s + c) > 0.
	 *
	 * To avoid branches, we calculate ((-(c + s)) >> 63) as the value to
	 * add to the output pointer (and to s), because its value will be 0
	 * iff c and s are both 0, and 1 otherwise.
	 */
.macro WRITE_DIGIT n
	mov	%r3, %r1
	rsh	%r3, 4 * (3 - \n)
	and	%r3, 0xf

	mov	%r4, %r3
	sub	%r4, 9
	neg	%r4
	rsh	%r4, 63
	mul	%r4, 'a' - '0' - 10
	add	%r4, '0'
	add	%r4, %r3
	stxb	[%r2 + 0], %r4

	add	%r3, %r0		/* %r3 = ((-(c + s)) >> 63) */
	neg	%r3
	rsh	%r3, 63

	add	%r2, %r3
	add	%r0, %r3
.endm

	WRITE_DIGIT 0
	WRITE_DIGIT 1
	WRITE_DIGIT 2
	WRITE_DIGIT 3

	/*
	 * It is possible that all digits are 0, in which case the output
	 * pointer did not advance from its initial value.  We do want a single
	 * 0 digit as output though.
	 *
	 * Since in this case, %r3 will be zero if all digits are zero, and 1
	 * otherwise, we can simply use %r0 + (%r3 ^ 1) to ensure that when all
	 * digits are 0, we retain the last one.
	 */
	xor	%r3, 1
	and	%r3, 1			/* Needed for older BPF verifiers */
	add	%r0, %r3
	exit
	.size	write_hex16, .-write_hex16

/*
 * void inet_ntoa6(const dt_dctx_t *dctx, const uint8_t *src, char *dst,
 *		   uint32 tbloff)
 */
	.align	4
	.global	dt_inet_ntoa6
	.type	dt_inet_ntoa6, @function
dt_inet_ntoa6:
	/*
	 * %r9		dst
	 * %r8		src
	 * %r7		bitmap of non-zero words
	 * %r6		dctx
	 * [%fp-4]	tbloff
	 *
	 * We make use of the fact that dst is a tstring which is known to be
	 * large enough to hold the longest output (OUTPUT_LEN = 40 bytes), and
	 * two copies of the input data (2 * INPUT_LEN = 32 bytes).
	 *
	 * We read the input data into (dst + OUTPUT_LEN + INPUT_LEN) and then
	 * copy it (after possibly applying a byte order conversion) to
	 * (dst + OUTPUT_LEN).  The unconverted copy is retained in case we
	 * fall back to using inet_ntoa().
	 */
	mov	%r9, %r3		/* %r9 = dst */
	mov	%r8, %r3
	add	%r8, OUTPUT_LEN		/* %r8 = converted copy */
	mov	%r6, %r1		/* %r6 = dctx */
	stxw	[%fp + -4], %r4		/* store tbloff */

	mov	%r3, %r2
	mov	%r2, INPUT_LEN
	mov	%r1, %r8
	add	%r1, INPUT_LEN		/* ptr to unconverted copy */
	call	BPF_FUNC_probe_read	/* probe_read(ptr, INPUT_LEN, src) */
	jne	%r0, 0, .Ldone

	/*
	 * Read the 8 words (16-bit values), build a bitmap in %r7 indicating
	 * which words are non-zero, and (after byte order conversion, if
	 * needed) store a copy of each word.
	 *
	 * We use an implementation that does not involve branches to reduce
	 * complexity for the BPF verifier.
	 *
	 * The IPv6 address has words in network byte order which may differ
	 * from the host byte order.  We store a 2nd copy of the words, with
	 * the byte order reversed (if needed).  We shouldn't need the 2nd copy
	 * if the byte order is the same but since the BPF verifier chokes on
	 * the output code below due to lack of tracking of relations between
	 * register values, the 2nd copy is needed anyway.
	 */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define NTOH(reg)
#else
# define NTOH(reg)	endbe	reg, 16
#endif
.macro GETWORD n
	ldxh	%r0, [%r8 + INPUT_LEN + \n * 2]
					/* Load word */
	NTOH(%r0)			/* Byte order conversion */
	stxh	[%r8 + \n * 2], %r0	/* Store word */
	neg	%r0			/* -word, 63th bit set if > 0 */
	rsh	%r0, 63			/* 1 if non-zero word */
	lsh	%r0, 7 - \n		/* Set n-th bit in bitmap ... */
	or	%r7, %r0		/* .. if non-zero word */
.endm

	mov	%r7, 0			/* Clear bitmap */
	GETWORD 0
	GETWORD 1
	GETWORD 2
	GETWORD 3
	GETWORD 4
	GETWORD 5
	GETWORD 6
	GETWORD 7

	/* Set the upper bound for %r7. */
	and	%r7, 0xff		/* Needed for BPF verifier */

	/*
	 * Handle mapped and embedded (compatible) IPv4 addresses.
	 *
	 * The exact semantics are a bit fuzzy in that neither RFC 4291 nor
	 * RFC 5952 address the case where a 6 zero-words are followed by 2
	 * words that do not form a valid IPv4 address.  Legacy DTrace takes
	 * the interpretation that a 6 zero-word prefix indicates an
	 * IPv4-compatible IPv6 address, whereas e.g. glibc requires that the
	 * last 2 words form a valid IPv4 address (i.e. first octet cannot be
	 * zero).
	 *
	 * The implementation here adopts the legacy approach:
	 *
	 * If any of the first 5 words is non-zero, not IPv4-in-IPv6.
	 * If 5 zero-words followed by 0xffff, IPv4.
	 * If 5 zero-words followed by a non-zero word, not IPv4-in-IPv6.
	 * If 6 zero-words followed by a non-zero word, IPv4.
	 * If 7 zero-words followed by anything other 0x0000 or 0x0001, IPv4.
	 * (7 zero-words followed by 0x0000 is the Unspecified Address.
	 *  7 zero-words followed by 0x0001 is the Loopnack Address.)
	 */
	mov	%r0, %r7
	and	%r0, 0xf8
	jne	%r0, 0, .Lnotipv4
	ldxh	%r0, [%r8 + 10]
	jeq	%r0, 0xffff, .Lipv4
	jne	%r0, 0, .Lnotipv4
	ldxh	%r0, [%r8 + 12]
	jne	%r0, 0, .Lipv4
	ldxh	%r0, [%r8 + 14]
	jgt	%r0, 1, .Lipv4
.Lnotipv4:

	/*
	 * Perform a table lookup to determine the location and length of the
	 * longest run of 0-words (if any).  The rodata map contains a 256-byte
	 * long table with precalculated (start, length) pairs encoded as
	 * (4-bit word index) << 4 | (4-bit word cunt).  The table is indexed
	 * by the bitmap value.
	 *
	 * Each value gives the word index of the longest run of zero-words
	 * contained in the IPv6 address that matches the bitmap value, if any.
	 * By IPv4 address representation convention (RFC 4291), only zero-word
	 * runs of length 2 or greater are collapsed.
	 *
	 * To aid the implementation of this function (and to reduce code
	 * complexity for the BPF verifier), bitmap values that do not contain
	 * any zero-word run of length 2 or more are given the value 0x70 to
	 * have the code below output the address in two parts: a 7 word
	 * prefix followed by a 1 word suffix.
	 */
	ldxdw	%r6, [%r6 + DCTX_RODATA]
	ldxw	%r1, [%fp + -4]		/* restore tbloff */
	lddw	%r0, RODATA_SIZE
	jge	%r1, %r0, .Ldone
	add	%r6, %r1		/* %r6 = dctx->rodata + tbloff */
	add	%r6, %r7
	ldxb	%r7, [%r6 + 0]		/* %r7 = tbl[%r7] */

	/*
	 * Determine the number of words to output at the start of the address.
	 * It is found in the upper 4 bits in %r7 (result of the table lookup
	 * above).  If the number of leading non-zero words is 7, the address
	 * does not allow for zero-word collapsing and we jump to code that
	 * simply outputs all 8 words.
	 */
	mov	%r6, %r7
	rsh	%r6, 4
	jge	%r6, 7, .Lfull

	/*
	 * Loop to output the first %r6 words of the address.  Each value is
	 * appended with a ':'.
	 */
.Lpref_loop:
	jle	%r6, 0, .Lpref_done
	ldxh	%r1, [%r8 + 0]
	mov	%r2, %r9
	call	write_hex16
	add	%r9, %r0
	stb	[%r9 + 0], ':'
	add	%r9, 1
	add	%r8, 2
	sub	%r6, 1
	ja	.Lpref_loop

.Lpref_done:
	/* Output another ':' in case a collapsed run of zero-words follows. */
	stb	[%r9 + 0], ':'

	/*
	 * Get the number of words output at the beginning of the address.  If
	 * there were no leading non-zero words, we advance the output pointer
	 * so that the ':' added above becomes the first of the '::' collapsed
	 * zero-words marker.  If any words were output, we keep the output
	 * pointer as-is so the next output will overwrite the ':'.
	 */
	mov	%r1, %r7
	rsh	%r1, 4			/* #(leading words) */
	mov	%r0, %r1
	neg	%r0
	rsh	%r0, 63
	xor	%r0, 1
	and	%r0, 1			/* Needed for older BPF verifiers */
	add	%r9, %r0

	/*
	 * Determine the number of collapsed zero-words.  We use a branch to
	 * help the BPF verifier place a range limit on %r1.
	 */
	mov	%r0, %r7
	and	%r0, 0xf		/* #(zero words to collapse) */
	add	%r1, %r0		/* #(words used) */
	jgt	%r1, 8, .Ldone

	/*
	 * Calculate the number of words left to output, and advance the input
	 * pointer to the start of the remaining words.
	 */
	mov	%r6, 8
	sub	%r6, %r1		/* #(words left to write) */
	mul	%r0, 2
	add	%r8, %r0

	/* Output ':' in case we end with a collapsed run of zero-words.  */
	stb	[%r9 + 0], ':'

	/*
	 * If there are no remaining non-zero words left to output, we need to
	 * advance the output pointer one byte to retain the ':' added above.
	 * If not, we keep the output pointer as-is (add 0) so the ':' will be
	 * overwritten.
	 */
	mov	%r0, %r6
	neg	%r0
	rsh	%r0, 63
	xor	%r0, 1
	and	%r0, 1			/* Needed for older BPF verifiers */
	add	%r9, %r0

	/*
	 * Loop to output the last %r6 words of the address.  Each value is
	 * prefixed with a ':'.
	 */
.Lpost_loop:
	jle	%r6, 0, .Ldone
	stb	[%r9 + 0], ':'
	add	%r9, 1
	ldxh	%r1, [%r8 + 0]
	mov	%r2, %r9
	call	write_hex16
	add	%r9, %r0
	add	%r8, 2
	sub	%r6, 1
	ja	.Lpost_loop

.Ldone:
	/* Output the terminating NUL byte and return. */
	stb	[%r9 + 0], 0
	mov	%r0, 0
	exit

.Lipv4:
	/* Output the last two words as an IPv4 address and return. */
	mov	%r1, %r6
	mov	%r2, %r8
	add	%r2, INPUT_LEN + 6 * 2	/* unconverted copy &words[6] */
	mov	%r3, %r9
	call	dt_inet_ntoa
	mov	%r0, 0
	exit

.Lfull:
	/* Output an IPv6 address without zero-cord collapsing. */
	mov	%r6, 7
.Lfull_loop:
	jle	%r6, 0, .Lfull_end
	ldxh	%r1, [%r8 + 0]
	mov	%r2, %r9
	call	write_hex16
	add	%r9, %r0
	stb	[%r9 + 0], ':'
	add	%r9, 1
	add	%r8, 2
	sub	%r6, 1
	ja	.Lfull_loop

.Lfull_end:
	ldxh	%r1, [%r8 + 0]
	mov	%r2, %r9
	call	write_hex16
	add	%r9, %r0
	ja	.Ldone
	.size	dt_inet_ntoa6, .-dt_inet_ntoa6
