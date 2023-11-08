// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Conceptually, the idea behind cleanpath() is relatively simple:
 * simplify gratuitous "//", "/./" and "/foo/../" sequences in a path name.
 *
 * In practice, the algorithm is a little tricky, all the more since we
 * want to implement this in BPF.
 *
 * Significantly, the BPF verifier really does not like conditional
 * statements, since it has to verify every conceivable execution path,
 * and the number of such paths can explode exponentially.  So, we go to
 * great lengths to replace conditional execution with nonconditional
 * execution.  For example,
 *      if (nup > 0)
 *      	nup--;
 * can be implemented as:
 *      tmp = (-nup) >> 63;
 *      nup -= tmp;
 *
 * Here is the high-level algorithm.  Milestones are designated with
 * "#" so that they can be found easily in the code.  Comments like
 * "execute if TMP==1" indicate which variable is set to 1 (or 0) to
 * indicate that a code block should be executed (or not).
 *
 *      // #1.  handle empty string: cleanpath("") = ""
 *      if (strlen(src) == 0) {
 *      	dst = "";
 *      	return;
 *      }
 *
 *      // #2.  handle single-char string: cleanpath("x") = "x", even for '.' and '/'
 *      if (strlen(src) == 1)
 *      	return;
 *
 *      lastchar = src[strlen(src) - 1];
 *
 *      // #3.  regularize processing by prepending and appending a '/'
 *      dst = '/' + dst + '/';
 *
 *      // #4.  loop forward to handle "//" and "/./" strings
 *      seq = 0;      // word whose bytes are the most recent chars we have seen
 *      for (to = from = 0; ; to++, from++) {    // "from" advances faster than "to"
 *      	char chr;
 *
 *      	chr = dst[from];
 *      	dst[to] = chr;
 *      	if (chr == '\0')
 *      		break;
 *      	seq = (seq << 8) | chr;
 *
 *      	if (recent two chars in seq are "//") {
 *      		// #5.  execute if TMP==1
 *      		to--;
 *      		seq >>= 8;
 *      	}
 *
 *      	if (recent three chars in seq are "/./") {
 *      		// #6.  execute if TMP==1
 *      		to -= 2;
 *      		seq >>= 16;
 *      	}
 *      }
 *
 *      // #7.  loop backward to handle "/../" strings
 *      nup = 0;      // how many ".." are unresolved
 *      brk = ito;    // index of last '/' we have seen
 *      seq = 0;      // word whose bytes are the most recent chars we have seen
 *      for (to = from = brk - 1; from >= 0; to--, from--) { // "from" descends faster than "to"
 *      	char chr;
 *
 *      	chr = dst[from];
 *      	dst[to] = chr;
 *      	seq = (seq << 8) | chr;
 *
 *      	if (recent four chars in seq are "/../") {  // drop 3 chars and increment nup
 *      		// #8.  execute if TMP==1
 *      		nup++;
 *      		to += 3;
 *      		seq >>= 24;
 *      	} else if (chr == '/') {        // completed a dir name that is not ".."
 *      		// #9.  execute if TMP==1
 *      		if (nup > 0) {          // we have to drop the dir name due to a ".."
 *      			// #10.  execute if TM4==1
 *      			to = brk;       // rewind to last brk index
 *      			nup--;          // decrement nup
 *      		} else {                // normal case, keep this dir name
 *      			// #11.  execute if TM2==1
 *      			brk = ito;      // note brk for future use
 *      		}
 *      	}
 *      }
 *
 *      // #12.  point to the first char in the output
 *      to++;
 *
 *      // #13.  for absolute paths, forget about nup:  "/.." is equivalent to "/"
 *      if (src[0] == '/')
 *      	goto Lout;
 *
 *      // #14.  for relative paths, advance past the initial, artificial '/'
 *      to++;
 *
 *      // #15.  for each nup, prepend a "../"
 *      while (nup > 0) {
 *      	ito -= 3;
 *      	dst[ITO + 0] = '.';
 *      	dst[ITO + 1] = '.';
 *      	dst[ITO + 2] = '/';
 *      	nup--;
 *      }
 *
 *      Lout:
 *
 *      // #16.  move the result string to the beginning of the dst buffer
 *      strcpy(dst, &dst[to]);
 *
 *      // #17.  check for empty string
 *      if (strlen(dst) < 1) {
 *      	dst = ".";
 *      	return;
 *      }
 *
 *      // #18.  check for single-char string
 *      if (strlen(dst) < 2)
 *      	return;
 *
 *      // #19.  strip off the trailing '/'
 *      dst[strlen(dst) - 1] = '\0';
 */

#define BPF_FUNC_probe_read	4
#define BPF_FUNC_probe_read_str	45

/*
 * void dt_cleanpath(const dt_dctx_t *dctx, char *src, char *dst);
 */
	.text
	.align	4
	.global	dt_cleanpath
	.type	dt_cleanpath, @function
dt_cleanpath :

#define DST %r9
	/* Cache dst pointer. */
	mov	DST, %r3

	/*
	 * Copy src to &dst[1].  Then we can directly dereference individual chars.
	 * The first byte, dst[0], is reserved for prepending a '/'.
	 */
	mov	%r3, %r2
	lddw	%r2, STRSZ
	add	%r2, 1
	mov	%r1, DST
	add	%r1, 1
	call	BPF_FUNC_probe_read_str
	/* At this point, %r0 has strlen(src) + 1. */

	/* #1.  Handle empty string: cleanpath("") = "". */
	jsgt	%r0, 1, .Lnotempty
	stb	[DST+0], 0
	mov	%r0, 0
	exit
.Lnotempty:

	/*
	 * #2.  Handle single-char string: cleanpath("x") = "x", even for '.' and '/'.
	 * Load the first char (which we had put in dst[1]) and stuff it in dst[0].
	 */
	ldxb	%r1, [DST+1]
	jgt	%r0, 2, .Lnotsinglechar
	stxb	[DST+0], %r1
	stb	[DST+1], 0
	mov	%r0, 0
	exit
.Lnotsinglechar:

#define FIRSTCHAR [%fp+-1]
	/* Save the first char. */
	stxb	FIRSTCHAR, %r1

	/* Save the last char. */
	mov	%r8, %r0
	add	%r8, DST
	ldxb	%r8, [%r8+-1]

	/* #3.  Prepend a '/'. */
	stb	[DST+0], '/'

	/* #3.  Append a '/'. */
	add	%r0, DST
	stb	[%r0+0], '/'
	stb	[%r0+1], 0

#define TMP %r8		/* temp reg */
#define ITO %r7		/* index to copy "to" */
#define IFR %r6		/* index to copy "from" */
#define SEQ %r5		/* sequence of most-recent chars */
#define CHR %r4		/* char being copied */
#define SIZ %r3		/* STRSZ */

	/* #4.  Loop forward through the string. */
	mov	ITO, 0
	mov	IFR, 0
	mov	SEQ, 0
	lddw	SIZ, STRSZ
.Lforward:
	jge	IFR, SIZ, .Lret		/* reassure the BPF verifier */

	/* Load the char from the "from" index. */
	mov	TMP, IFR
	add	TMP, DST
	ldxb	CHR, [TMP + 0]

	/* Store the char to the "to" index. */
	mov	TMP, ITO
	jlt	TMP, 0, .Lret		/* reassure the BPF verifier */
	jgt	TMP, IFR, .Lret		/* reassure the BPF verifier */
	add	TMP, DST
	stxb	[TMP + 0], CHR

	/* If the char is '\0', break out of the loop. */
	jeq	CHR, 0, .Lforward_brk

	/* Add this char to the sequence of chars. */
	lsh	SEQ, 8
	or	SEQ, CHR

	/* #5.  Compute TMP = 1 (if we should execute), else 0. */
	mov	TMP, (('/' << 8) | '/')
	xor	TMP, SEQ
	and	TMP, 0xffff
	neg	TMP
	rsh	TMP, 63
	xor	TMP, 1

	/* Use this to back up one char if "//". */
	sub	ITO, TMP

	/* Update SEQ accordingly. */
	mul	TMP, 8
	rsh	SEQ, TMP

	/* #6.  Compute TMP = 1 (if we should execute), else 0. */
	mov	TMP, (('/' << 16) | ('.' << 8) | '/')
	xor	TMP, SEQ
	and	TMP, 0xffffff
	neg	TMP
	rsh	TMP, 63
	xor	TMP, 1

	/* Use this to back up two chars if "/./". */
	mul	TMP, 2
	sub	ITO, TMP

	/* Update SEQ accordingly. */
	mul	TMP, 8
	rsh	SEQ, TMP

	/* Advance the indices and iterate. */
	add	ITO, 1
	add	IFR, 1
	ja	.Lforward
#undef SIZ

.Lforward_brk:

#define TM2 %r3		/* another temp reg */
#define TM3 %r2		/* another temp reg */
#define BRK %r1		/* index of last directory-level break '/' */
#define NUP %r0		/* num of parent directories to go up due to "../" */

	/* #7.  Handle double-dot "/../", looping backward. */
	mov	NUP, 0
	mov	SEQ, 0
	mov	BRK, ITO
	jslt	BRK, 1, .Lret		/* reassure the BPF verifier */
	lddw	TMP, STRSZ
	jge	BRK, TMP, .Lret		/* reassure the BPF verifier */
	mov	IFR, BRK
	sub	IFR, 1
	mov	ITO, IFR
.Lbackward:
	/* Load the char from the "from" index. */
	mov	TMP, IFR
	add	TMP, DST
	ldxb	CHR, [TMP + 0]

	/* Store the char to the "to" index. */
	mov	TMP, ITO
	lddw	TM2, STRSZ
	jge	TMP, TM2, .Lret		/* reassure the BPF verifier */
	jlt	TMP, IFR, .Lret		/* reassure the BPF verifier */
	add	TMP, DST
	stxb	[TMP + 0], CHR

	/* Add this char to the sequence of chars. */
	lsh	SEQ, 8
	or	SEQ, CHR

	/* #8.  Compute TMP = 1 (if we should execute), else 0. */
	mov	TMP, ('/' | ('.' << 8) | ('.' << 16) | ('/' << 24))
	xor	TMP, SEQ
	lsh	TMP, 32
	rsh	TMP, 32
	neg	TMP
	rsh	TMP, 63
	xor	TMP, 1

	/* Use this to increase NUP if "/../". */
	add	NUP, TMP

	/* Use this to back up three chars if "/../". */
	mov	TM2, TMP
	mul	TM2, 3
	add	ITO, TM2

	/* Update SEQ accordingly. */
	mul	TM2, 8
	rsh	SEQ, TM2

	/* Now switch to the not-"/.../" case. */
	xor	TMP, 1

/* Rename CHR's reg TM4;  TM4 is yet another temp reg and starts with CHR's value. */
#undef CHR
#define TM4 %r4

	/* If the last char is "/", TM4 = 1.  Else, TM4 = 0. */
	xor	TM4, '/'
	neg	TM4
	rsh	TM4, 63
	xor	TM4, 1

	/* #9.  Compute TMP = 1 (if we should execute), else 0. */
	mul	TMP, TM4

	/* If NUP > 0, TM2 = 1.  Else, TM2 = 0. */
	mov	TM2, NUP
	neg	TM2
	rsh	TM2, 63

	/* #10.  Compute TM4 = 1 (if we should execute), else 0. */
	mov	TM4, TMP
	mul	TM4, TM2

	/* If TM4 is 1, then ITO = BRK (revert to last BRK index). */
	mov	TM3, BRK
	sub	TM3, ITO
	mul	TM3, TM4
	add	ITO, TM3

	/* If TM4 is 1, then NUP--. */
	sub	NUP, TM4

	/* #11.  Compute TM2 = 1 (if we should execute), else 0. */
	xor	TM2, 1
	mul	TM2, TMP

	/* If TM2 is 1, then BRK = ITO. */
	mov	TM4, ITO
	sub	TM4, BRK
	mul	TM4, TM2
	add	BRK, TM4

	/* Advance the indices and iterate. */
	sub	ITO, 1
	sub	IFR, 1
	jsge	IFR, 0, .Lbackward
#undef BRK
#undef TM4
#undef TM2
#undef SEQ
#undef TM3
#undef IFR

	/*
	 * At this point, we've finished the backward loop.
	 */

	/* #12.  Increment ITO to point to the beginning of the output. */
	add	ITO, 1

	/* #13.  If we have an absolute path (src[0] == '/'), go to Lout. */
	ldxb	TMP, FIRSTCHAR
	jeq	TMP, '/', .Lout
#undef FIRSTCHAR

	/* #14.  Otherwise, advance past the initial, spurious '/'. */
	add	ITO, 1

	/* #15.  Add a "../" prefix NUP times. */
	/*
	 * FIXME:  The BPF verifier needs some assurance that this loop
	 * is not endless.  We arbitrarily assume NUP is at most 15,
	 * equivalent to some clean path of the form
	 * "../../../../../../../../../../../../../../../foo/bar".
	 */
	and	NUP, 15

	lddw	TMP, STRSZ
	jgt	ITO, TMP, .Lret		/* reassure the BPF verifier */
.Lprefix:
	jsle	NUP, 0, .Lout

	sub	ITO, 3
	jslt	ITO, 0, .Lret		/* reassure the BPF verifier */

	mov	TMP, ITO
	add	TMP, DST
	stb	[TMP+0], '.'
	stb	[TMP+1], '.'
	stb	[TMP+2], '/'

	sub	NUP, 1
	ja	.Lprefix
#undef NUP
#undef TMP

.Lout:
	jslt	ITO, 0, .Lret		/* reassure the BPF verifier */
	lddw	%r1, STRSZ
	jgt	ITO, %r1, .Lret		/* reassure the BPF verifier */

	/* #16.  Copy from index "ito" to the beginning of the DST buffer. */
	/*
	 * FIXME:  Is this okay?  We are copying a string in the DST
	 * array from offset ITO down to offset 0.  The input and output
	 * strings might (indeed probably will) overlap.  Can we use
	 * the BPF helper function in this way?
	 */
	mov	%r1, DST
	lddw	%r2, STRSZ
	add	%r2, 1
	sub	%r2, ITO
	mov	%r3, DST
	add	%r3, ITO
	call	BPF_FUNC_probe_read_str
	/* At this point, %r0 has strlen(dst) + 1. */
#undef ITO

	/* #17.  If the string is empty, make it ".\0" and return. */
	jsge	%r0, 2, .Lnotempty2
	sth	[DST+0], '.' /* little-endian uses "\0.", or just '.' */
	ja	.Lret
.Lnotempty2:

	/* #18.  If the string is a single char (plus NUL char), then return. */
	jle	%r0, 2, .Lret

	/* #19.  Strip off the trailing '/'. */
	add	%r0, DST
	stb	[%r0+-2], 0
#undef DST

.Lret:
	mov	%r0, 0
	exit
	.size	dt_cleanpath, .-dt_cleanpath
