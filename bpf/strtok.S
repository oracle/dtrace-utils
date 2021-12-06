// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */

#define DT_STRLEN_BYTES		2

#define BPF_FUNC_probe_read	4
#define BPF_FUNC_probe_read_str	45

	.text

/*
 * The algorithm stores information about the delimiters in a table for
 * 256 ASCII chars, each table entry a single bit indicating whether the
 * char is a delimiter.  This version stores the 256-bit (32-byte) table
 * on the stack as four, 8-byte bins.  The bit for char c is bit c&63 in
 * bin c>>6.  The bins are stored at:
 *     bin 0    [TAB+- 8]
 *     bin 1    [TAB+-16]
 *     bin 2    [TAB+-24]
 *     bin 3    [TAB+-32]
 * where TAB will be dt_strtok()'s FP.
 */

/* ================================================== */

/*
 * // Initialize the delimiter table (place a nonzero bit for each delimiter found).
 * // Note that:
 * //   - we expect the delimiter string to be NULL-terminated
 * //   - the NULL byte is always going to be set in the delimiter table
 * //   - we also pass a "length" argument in to help the BPF verifier
 * void dt_strtok_init_table(uint64_t *tab, char *del, uint64_t len)
 * {
 *     idx = 0
 * Linit_table:
 *     if (idx >= len) return
 *     chr = del[idx]
 *     bit = (1 << (chr & 63))
 *     bin = chr >> 6
 *     tab[bin] |= bit
 *     if (chr == '\0') return
 *     idx++
 *     goto Linit_table
 * }
 */
	.align	4
	.global	dt_strtok_init_table
	.type	dt_strtok_init_table, @function
dt_strtok_init_table:
#define TAB %r1
#define DEL %r2
#define LEN %r3
#define IDX %r4
#define CHR %r5
#define BIN %r6
#define BIT %r7
#define TMP %r8
#define USE %r9
	mov	IDX, 0			/* idx = 0 */

.Linit_table:
	jlt	IDX, LEN, 1		/* if (idx >= len) return */
	exit

	mov	CHR, DEL
	add	CHR, IDX
	ldxb	CHR, [CHR+0]		/* chr = del[idx] */
	and	CHR, 0xff

	mov	BIT, 1			/* bit = (1 << (chr & 63)) */
	mov	TMP, CHR
	and	TMP, 63
	lsh	BIT, TMP

	mov	BIN, CHR		/* bin = chr >> 6 */
	rsh	BIN, 6

	/* tab[bin] |= bit */
	/*
	 * Since the table is on the stack and we cannot use variable
	 * offsets into the stack, we iterate over all four bins:
	 *     tab[- 8] |= (0 == bin ? 1 : 0) * bit
	 *     tab[-16] |= (1 == bin ? 1 : 0) * bit
	 *     tab[-24] |= (2 == bin ? 1 : 0) * bit
	 *     tab[-32] |= (3 == bin ? 1 : 0) * bit
	 *
	 * Further, since the BPF verifier is easily stymied by conditional
	 * branching, we compute the conditionalized operators with:
	 *                         //    try == bin    try != bin
	 *     use = (try ^ bin)   //    use == 0      use >  0
	 *     use--               //    use <  0      use >= 0
	 *     use >>= 63          //    use == 1      use == 0
	 *     tab[off] |= use * bit
	 */
.macro macro_init try off
	mov	USE, BIN
	xor	USE, \try
	sub	USE, 1
	rsh	USE, 63
	mul	USE, BIT

	ldxdw	TMP, [TAB+\off]
	or	TMP, USE
	stxdw	[TAB+\off], TMP
.endm
	macro_init 0,-8
	macro_init 1,-16
	macro_init 2,-24
	macro_init 3,-32

	jne	CHR, 0, 1		/* if (chr == '\0') return */
	exit

	add	IDX, 1			/* idx++ */

	ja	.Linit_table		/* goto Linit_table */
#undef TAB
#undef DEL
#undef LEN
#undef IDX
#undef CHR
#undef BIN
#undef BIT
#undef TMP
#undef USE
	.size	dt_strtok_init_table, .-dt_strtok_init_table

/* ================================================== */

/*
 * // Look up each byte, replacing it by 1 or 0 using the table.
 * void dt_strtok_lookup(uint64_t *tab, char *str, uint64_t len)
 * {
 *     idx = 0
 * Llookup:
 *     if (idx >= len) return
 *     ptr = &str[idx]
 *     bin = *ptr >> 6
 *     val = tab[bin]
 *     val >>= *ptr & 63
 *     val &= 1
 *     *ptr = val
 *     idx++
 *     goto Llookup
 * }
 */
	.align	4
	.global	dt_strtok_lookup
	.type	dt_strtok_lookup, @function
dt_strtok_lookup:

#define VAL %r0
#define TAB %r1
#define STR %r2
#define LEN %r3
#define BIN %r4
#define IDX %r5
#define PTR %r6
#define TMP %r7
#define USE %r8

	mov	IDX, 0			/* idx = 0 */

.Llookup:
	jlt	IDX, LEN, 1		/* if (idx >= len) return */
	exit

	mov	PTR, STR
	add	PTR, IDX		/* ptr = &str[idx] */

	ldxb	BIN, [PTR+0]
	and	BIN, 255
	rsh	BIN, 6			/* bin = *ptr >> 6 */

	/* val = tab[bin] */
	/*
	 * The tab[bin] lookup is tricky for the same reasons as
	 * described for dt_strtok_init_table().  This time, we use
	 *     val = 0
	 *     val += (0 == bin ? 1 : 0) * tab[- 8]
	 *     val += (1 == bin ? 1 : 0) * tab[-16]
	 *     val += (2 == bin ? 1 : 0) * tab[-24]
	 *     val += (3 == bin ? 1 : 0) * tab[-32]
	 * with each conditionalized operator replaced by
	 *                         //    try == bin    try != bin
	 *     use = (try ^ bin)   //    use == 0      use >  0
	 *     use--               //    use <  0      use >= 0
	 *     use >>= 63          //    use == 1      use == 0
	 *     val += use * tab[off]
	 */
	mov	VAL, 0			/* val = 0 */
.macro macro_look try off
	mov	USE, \try
	xor	USE, BIN		/* use = try ^ bin */
	sub	USE, 1			/* use-- */
	rsh	USE, 63			/* use >>= 63 */
	ldxdw	TMP, [TAB+\off]
	mul	TMP, USE
	add	VAL, TMP		/* val += use * tab[try] */
.endm
	macro_look 0,-8
	macro_look 1,-16
	macro_look 2,-24
	macro_look 3,-32

	ldxb	TMP, [PTR+0]
	and	TMP, 63
	rsh	VAL, TMP		/* val >>= *ptr & 63 */

	and	VAL, 1			/* val &= 1 */

	stxb	[PTR+0], VAL		/* *ptr = val */

	add	IDX, 1			/* idx++ */

	ja	.Llookup		/* goto Llookup */
#undef VAL
#undef TAB
#undef STR
#undef LEN
#undef BIN
#undef IDX
#undef PTR
#undef TMP
#undef USE
	.size	dt_strtok_lookup, .-dt_strtok_lookup

/* ================================================== */

/*
 * // Each byte is 1 or 0, flip its value.
 * // Do 8 bytes at a time; len must be a multiple of 8.
 * void dt_strtok_flip(char *str, uint64_t len)
 * {
 *     msk = 0x0101010101010101
 *     idx = 0
 * Lflip:
 *     if (idx >= len) return
 *     ptr = (uint64_t*)&str[idx]
 *     val = *ptr
 *     val ^= msk
 *     *ptr = val
 *     idx += 8
 *     goto Lflip
 * }
 */
	.align	4
	.global	dt_strtok_flip
	.type	dt_strtok_flip, @function
dt_strtok_flip:

#define STR %r1
#define LEN %r2
#define MSK %r3
#define IDX %r4
#define PTR %r5
#define VAL %r6

	lddw	MSK, 0x0101010101010101	/* msk = 0x0101010101010101 */
	mov	IDX, 0			/* idx = 0 */

.Lflip:
	jlt	IDX, LEN, 1		/* if (idx >= siz) return */
	exit

	mov	PTR, STR
	add	PTR, IDX		/* ptr = (uint64_t*)&str[idx] */

	ldxdw	VAL, [PTR+0]		/* val = *ptr */
	xor	VAL, MSK		/* val ^= msk */
	stxdw	[PTR+0], VAL		/* *ptr = val */

	add	IDX, 8			/* idx += 8 */

	ja	.Lflip			/* goto Lflip */
#undef STR
#undef LEN
#undef MSK
#undef IDX
#undef PTR
#undef VAL
	.size	dt_strtok_flip, .-dt_strtok_flip

/* ================================================== */

/*
 * // str has an 8-byte prefix giving the offset to start at.
 * // So the actual string starts at str+8+*((uint64_t*)str).
 * void dt_strtok(char *dst, char *str, const char *del, char *tmp)
 * {
 *     // discard delimiter length prefix; we look for the NULL terminator anyhow
 *     del += DT_STRLEN_BYTES
 *
 *     // len = roundup(STRSZ + 1, 8)
 *     len = ((STRSZ + 1) + 7) & -8
 *
 *     // zero the delimiter table (do here so we see the stack usage)
 *     *((uint64_t *)(*fp +  -8)) = 0
 *     *((uint64_t *)(*fp + -16)) = 0
 *     *((uint64_t *)(*fp + -24)) = 0
 *     *((uint64_t *)(*fp + -32)) = 0
 *
 *     // initialize the delimiter table
 *     // Note:
 *     //   - we provide "len" as a hint to the BPF verifier
 *     //   - we copy "del" into "tmp" to guarantee "len" room
 *     bpf_probe_read_str(tmp, STRSZ + 1, del)
 *     dt_strtok_init_table(fp, tmp, len)
 *
 *     // copy str into tmp
 *     end = bpf_probe_read_str(tmp, STRSZ + 1, str+8+*((uint64_t*)str))
 *     end--
 *
 *     // check each byte against delimiter table
 *     dt_strtok_lookup(fp, tmp, len)
 *
 *     // find first non-delimiting char
 *     bgn = bpf_probe_read_str(dst, STRSZ + 1, tmp)
 *     if (bgn s<= 0) goto Lnull
 *     bgn--
 *
 *     // make sure there is at least one char
 *     if (bgn >= end) goto Lnull
 *
 *     // flip each byte
 *     dt_strtok_flip(tmp, len)
 *
 *     // find actual length
 *     len = bpf_probe_read_str(dst, STRSZ + 1 - bgn, tmp + bgn)
 *     if (len s<= 0) goto Lnull
 *     len--
 *
 *     // adjust bgn to account for strtok offset
 *     bgn += *((uint64_t*)str)
 *
 *     // copy str + bgn to destination
 *     dt_strlen_store(len, dst)
 *     bpf_probe_read(dst + DT_STRLEN_BYTES, len, str + 8 + bgn)
 *     dst[DT_STRLEN_BYTES + len] = '\0'
 *
 *     // update the 8-byte prefix (strtok offset)
 *     bgn += len
 *     *((uint64_t*)str) = bgn
 *
 *     return dst
 *
 * Lnull:
 *     // advance the strtok offset to the end and return 0
 *     *((uint64_t*)str) = end
 *     return 0
 * }
 */

	.align	4
	.global	dt_strtok
	.type	dt_strtok, @function
dt_strtok:
/* the stack has the delimiter table starting at %fp+-32 */
#define DST_X [%fp+-40]
#define END_X [%fp+-48]
#define STR %r6
#define DEL %r7
#define LEN %r8
#define TMP %r9

	/* stash variables */
	stxdw	DST_X, %r1
	mov	STR, %r2
	mov	DEL, %r3
	mov	TMP, %r4

	/* discard delimiter length prefix; we look for the NULL terminator anyhow */
	add	DEL, DT_STRLEN_BYTES	/* del += DT_STRLEN_BYTES */

	/* len = roundup(STRSZ + 1, 8) */
	lddw	LEN, STRSZ
	add	LEN, 8
	and	LEN, -8			/* len = ((STRSZ + 1) + 7) & -8 */

	/* zero the delimiter table (do here so we see the stack usage) */
	mov	%r1, 0
	stxdw	[%fp+- 8], %r1		/* *((uint64_t *)(*fp +  -8)) = 0 */
	stxdw	[%fp+-16], %r1		/* *((uint64_t *)(*fp + -16)) = 0 */
	stxdw	[%fp+-24], %r1		/* *((uint64_t *)(*fp + -24)) = 0 */
	stxdw	[%fp+-32], %r1		/* *((uint64_t *)(*fp + -32)) = 0 */

	/* initialize the delimiter table */
	mov	%r1, TMP
	lddw	%r2, STRSZ
	add	%r2, 1
	mov	%r3, DEL
	call	BPF_FUNC_probe_read_str	/* bpf_probe_read_str(tmp, STRSZ + 1, del) */
#undef DEL
	mov	%r1, %fp
	mov	%r2, TMP
	mov	%r3, LEN
	call	dt_strtok_init_table	/* dt_strtok_init_table(fp, tmp, len) */

	/* copy str into tmp */
	mov	%r1, TMP
	lddw	%r2, STRSZ
	add	%r2, 1
	mov	%r3, STR
	add	%r3, 8
	ldxdw	%r4, [STR+0]
	jsge	%r4, 0, 1
	mov	%r4, 0
	add	%r3, %r4
	call	BPF_FUNC_probe_read_str	/* end = bpf_probe_read_str(tmp, STRSZ + 1, str+8+*((uint64_t*)str)) */
	sub	%r0, 1			/* end-- */
	stxdw	END_X, %r0

	/* check each byte against delimiter table */
	mov	%r1, %fp
	mov	%r2, TMP
	mov	%r3, LEN
	call	dt_strtok_lookup	/* dt_strtok_lookup(fp, tmp, len) */

	/* find first non-delimiting char */
#define BGN %r7
	ldxdw	%r1, DST_X
	lddw	%r2, STRSZ
	add	%r2, 1
	mov	%r3, TMP
	call	BPF_FUNC_probe_read_str	/* bgn = bpf_probe_read_str(dst, STRSZ + 1, tmp) */
	mov	BGN, %r0
	jsle	BGN, 0, .Lnull		/* if (bgn s<= 0) goto Lnull */
	sub	BGN, 1			/* bgn-- */

	/* make sure there is at least one char */
	ldxdw	%r1, END_X
	jge	BGN, %r1, .Lnull	/* if (bgn >= end) goto Lnull */

	/* flip each byte */
	mov	%r1, TMP
	mov	%r2, LEN
	call	dt_strtok_flip		/* dt_strtok_flip(tmp, len) */

	/* find actual length */
	ldxdw	%r1, DST_X
	lddw	%r2, STRSZ
	add	%r2, 1
	sub	%r2, BGN
	mov	%r3, TMP
	add	%r3, BGN
	call	BPF_FUNC_probe_read_str	/* len = bpf_probe_read_str(dst, STRSZ + 1 - bgn, tmp + bgn) */
	mov	LEN, %r0
	jsle	LEN, 0, .Lnull		/* if (len s<= 0) goto Lnull */
	sub	LEN, 1			/* len-- */

	/* adjust bgn to account for strtok offset */
	ldxdw	%r1, [STR+0]
	add	BGN, %r1		/* bgn += *((uint64_t*)str) */

	/* (help the BPF verifier) */
	jsge	BGN, 0, 1
	mov	BGN, 0

	/* copy str + bgn to destination */
	mov	%r1, LEN
	ldxdw	%r2, DST_X
	call	dt_strlen_store		/* dt_strlen_store(len, dst) */

	ldxdw	%r1, DST_X
	add	%r1, DT_STRLEN_BYTES
	mov	%r2, LEN
	mov	%r3, STR
	add	%r3, 8
	add	%r3, BGN
	call	BPF_FUNC_probe_read	/* bpf_probe_read(dst + DT_STRLEN_BYTES, len, str + 8 + bgn) */

	mov	%r1, 0
	ldxdw	%r2, DST_X
	add	%r2, LEN
	stxb	[%r2+DT_STRLEN_BYTES], %r1
					/* dst[DT_STRLEN_BYTES + len] = '\0' */

	/* update the 8-byte prefix (strtok offset) */
	add	BGN, LEN		/* bgn += len */
	stxdw	[STR+0], BGN		/* *((uint64_t*)str) = bgn */

	ldxdw	%r0, DST_X		/* return dst */
	exit

	/* advance the strtok offset to the end and return 0 */
.Lnull:
	ldxdw	%r0, END_X
	stxdw	[STR+0], %r0		/* *((uint64_t*)str) = end */
	mov	%r0, 0			/* return 0 */
	exit
#undef STR
#undef TMP
#undef LEN
#undef BGN
#undef DST_X
	.size	dt_strtok, .-dt_strtok
