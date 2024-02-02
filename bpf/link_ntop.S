// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates.
 */

#include <bpf_asm_helpers.h>

#define ARPHRD_ETHER		1	/* ARP protocol hw identifier <linux/if_arp.h> */
#define ARPHRD_INFINIBAND	32	/* ARP protocol hw identifier <linux/if_arp.h> */

#define ETH_ALEN		6	/* Octets in ethernet addr <linux/if_ether.h> */
#define INFINIBAND_ALEN		20	/* Octets in IPoIB HW addr <linux/if_infiniband.h> */

#define MAX_ALEN		20

#define LENGTH			72

	.text

/*
 * uint64_t write_byte2hex(const uint8_t src, char *dst)
 */
	.align	4
	.global	write_byte2hex
	.type	write_byte2hex, @function
write_byte2hex:
	/*
	 * Branch-free implementation of byte-to-hex function.
	 *
	 * Process 4 bits (one hex digit) at a time.
	 *
	 * Given: r3 in [0, 15].
	 * Then, ((-(r3 - 9)) >> 63) is 1 if r3 in [10, 15], and 0 otherwise.
	 * Therefore, the hex digit (character) representing r3 is:
	 *	r3 + '0' + ((-(r3 - 9)) >> 63) * ('a' - '0' - 10)
	 */

.macro WRITE_DIGIT n
	/* Put digit n from %r1 into %r3. */
	mov	%r3, %r1
	rsh	%r3, 4 * (1 - \n)
	and	%r3, 0xf

	/* Store the converted value into %r2. */
	mov	%r4, %r3
	sub	%r4, 9
	neg	%r4
	rsh	%r4, 63
	mul	%r4, 'a' - '0' - 10
	add	%r4, '0'
	add	%r4, %r3
	stxb	[%r2 + \n], %r4
.endm

	WRITE_DIGIT 0
	WRITE_DIGIT 1

	exit
	.size	write_byte2hex, .-write_byte2hex

/*
 * void link_ntop(const dt_dctx_t *dctx, const uint8_t *src, char *dst, uint32 type)
 */
	.align	4
	.global	dt_link_ntop
	.type	dt_link_ntop, @function
dt_link_ntop:
	/*
	 * %r9		dst pointer
	 * %r8		src copy
	 * %r7		len left
	 *
	 * We copy the src string so that we can safely dereference individual
	 * bytes.  The src string is at most MAX_ALEN bytes.  The dst string is
	 * three times the src length.  The dst space is a tstring, which is
	 * known to be at least 72 bytes.  This is long enough for both src copy
	 * and dst string since, as we write the dst string, src bytes are
	 * being used up.  So the dst string will start at byte 0 while src
	 * will be copied to offset LENGTH-MAX_ALEN.
	 */

	/* Write pointer to dst. */
	mov	%r9, %r3

	/* Copy of src data. */
	mov	%r8, %r3
	add	%r8, LENGTH - MAX_ALEN

	/* Store len in %r7 based on arg4. */
	mov	%r7, ETH_ALEN
	jeq	%r4, ARPHRD_INFINIBAND, .Lib
	jne	%r4, ARPHRD_ETHER, .Lerror
	ja	.Lcomp
.Lib:
	mov	%r7, INFINIBAND_ALEN
.Lcomp:

	/* Copy src data. */
	mov	%r3, %r2
	mov	%r2, %r7
	mov	%r1, %r8
	call	BPF_FUNC_probe_read
	jne	%r0, 0, .Lerror

	/* Loop over bytes.  Each byte is appended with a ':'. */
.Lloop:
	jle	%r7, 0, .Ldone
	ldxb	%r1, [%r8 + 0]
	mov	%r2, %r9
	call	write_byte2hex
	stb	[%r9 + 2], ':'
	add	%r9, 3
	add	%r8, 1
	sub	%r7, 1
	ja	.Lloop
.Ldone:
	stb	[%r9 + -1], 0
	mov	%r0, 0
	exit

.Lerror:
	/* Output the terminating NUL byte and return. */
	stb	[%r9 + 0], 0
	mov	%r0, 0
	exit

	.size	dt_link_ntop, .-dt_link_ntop
