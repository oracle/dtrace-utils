/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *
 * Test different inline assignments by various expressions.
 *
 * SECTION: Type and Constant Definitions/Inlines
 *
 * NOTES: The commented lines for the floats and doubles should be uncommented
 * once the functionality is implemented.
 *
 */

#pragma D option quiet


inline char new_char = 'c' + 2;
inline short new_short = 10 * new_char;
inline int new_int = 100 + new_short;
inline long new_long = 1234567890;
inline long long new_long_long = 1234512345 * new_long;
inline int8_t new_int8 = 'p';
inline int16_t new_int16 = 20 / new_int8;
inline int32_t new_int32 = 200;
inline int64_t new_int64 = 2000000 * (-new_int16);
inline intptr_t new_intptr = 0x12345 - 129;
inline uint8_t new_uint8 = 'q';
inline uint16_t new_uint16 = 30 - new_uint8;
inline uint32_t new_uint32 = 300 - 0;
inline uint64_t new_uint64 = 3000000;
inline uintptr_t new_uintptr = 0x67890 / new_uint64;

/* inline float new_float = 1.23456;
inline double new_double = 2.34567890;
inline long double new_long_double = 3.567890123;
*/

inline unsigned long * pointer = &`max_pfn;
inline int result = 3 > 2 ? 3 : 2;

BEGIN
{
	printf("new_char: %c\nnew_short: %d\nnew_int: %d\nnew_long: %d\n",
	    new_char, new_short, new_int, new_long);
	printf("new_long_long: %d\nnew_int8: %d\nnew_int16: %d\n",
	    new_long_long, new_int8, new_int16);
	printf("new_int32: %d\nnew_int64: %d\n", new_int32, new_int64);
	printf("new_intptr: %d\nnew_uint8: %d\nnew_uint16: %d\n",
	    new_intptr, new_uint8, new_uint16);
	printf("new_uint32:%d\nnew_uint64: %d\nnew_uintptr:%d\nresult:%d",
	    new_uint32, new_uint64, new_uintptr, result);
	exit(0);
}
