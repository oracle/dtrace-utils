/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_VARINT_H
#define	_DT_VARINT_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Variable-length integers
 *
 * These functions convert between uint64_t integers and strings of 1-9
 * bytes.  The first 1<<7 integers are stored in a single byte with leading
 * bit 0.  The next 1<<14 integers are stored in two bytes with leading bits
 * 1 and 0.  And so on.  Here are the ranges of integers:
 *
 *      minimum integer    # of    leading    # of bits
 *        in this range    bytes     byte     left over       # of integers
 *                                (in bits)   for values      in this range
 *
 *                    0      1     0???????    1*8-1= 7 0x80
 *                 0x80      2     10??????    2*8-2=14 0x4000
 *               0x4080      3     110?????    3*8-3=21 0x200000
 *             0x204080      4     1110????    4*8-4=28 0x10000000
 *           0x10204080      5     11110???    5*8-5=35 0x800000000
 *          0x810204080      6     111110??    6*8-6=42 0x40000000000
 *        0x40810204080      7     1111110?    7*8-7=49 0x2000000000000
 *      0x2040810204080      8     11111110    8*8-8=56 0x100000000000000
 *    0x102040810204080      9     11111111    9*8-8=64        1 << 64
 *
 * If n is the number of bytes:
 *   VARINT_$n_PREFIX  =  leading byte (with 0 for ?), shown above
 *   VARINT_$n_PLIM    =  VARINT_${n+1}_PREFIX
 *   VARINT_$n_SHIFT   =  8*(n-1)
 *   VARINT_$n_MIN     =  VARINT_${n-1}_MAX + 1   with VARINT_1_MIN = 0
 *   VARINT_$n_MAX     =  inclusive maximum
 *
 * Notice that since we go up to at most 9 bytes.  So if the first 8 bits
 * are 1s, we we know the next 8 bytes represent the value.
 */
#define VARINT_HI_MASK(b)	((uint8_t)~(b))

#define VARINT_1_PREFIX		(uint8_t)0x00
#define VARINT_1_PLIM		0x80
#define VARINT_1_SHIFT		0
#define VARINT_1_MIN		0
#define VARINT_1_MAX		0x7f

#define VARINT_2_PREFIX		(uint8_t)0x80
#define VARINT_2_PLIM		0xc0
#define VARINT_2_SHIFT		8
#define VARINT_2_MIN		(VARINT_1_MAX + 1)
#define VARINT_2_MAX		0x407f

#define VARINT_3_PREFIX		(uint8_t)0xc0
#define VARINT_3_PLIM		0xe0
#define VARINT_3_SHIFT		16
#define VARINT_3_MIN		(VARINT_2_MAX + 1)
#define VARINT_3_MAX		0x20407f

#define VARINT_4_PREFIX		(uint8_t)0xe0
#define VARINT_4_PLIM		0xf0
#define VARINT_4_SHIFT		24
#define VARINT_4_MIN		(VARINT_3_MAX + 1)
#define VARINT_4_MAX		0x1020407f

#define VARINT_5_PREFIX		(uint8_t)0xf0
#define VARINT_5_PLIM		0xf8
#define VARINT_5_SHIFT		32
#define VARINT_5_MIN		(VARINT_4_MAX + 1)
#define VARINT_5_MAX		0x081020407f

#define VARINT_6_PREFIX		(uint8_t)0xf8
#define VARINT_6_PLIM		0xfc
#define VARINT_6_SHIFT		40
#define VARINT_6_MIN		(VARINT_5_MAX + 1)
#define VARINT_6_MAX		0x04081020407f

#define VARINT_7_PREFIX		(uint8_t)0xfc
#define VARINT_7_PLIM		0xfe
#define VARINT_7_SHIFT		48
#define VARINT_7_MIN		(VARINT_6_MAX + 1)
#define VARINT_7_MAX		0x0204081020407f

#define VARINT_8_PREFIX		(uint8_t)0xfe
#define VARINT_8_PLIM		0xff
#define VARINT_8_SHIFT		56
#define VARINT_8_MIN		(VARINT_7_MAX + 1)
#define VARINT_8_MAX		0x010204081020407f

#define VARINT_9_PREFIX		(uint8_t)0xff
#define VARINT_9_PLIM		0xff
#define VARINT_9_SHIFT		0
#define VARINT_9_MIN		(VARINT_8_MAX + 1)
#define VARINT_9_MAX		0xffffffffffffffff

#define VARINT_MAX_BYTES	9

extern uint64_t dt_int2vint(uint64_t num, char *str);
extern uint64_t dt_vint2int(const char *str);
extern uint64_t dt_vint_size(uint64_t val);
extern const char *dt_vint_skip(const char *str);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_VARINT_H */
