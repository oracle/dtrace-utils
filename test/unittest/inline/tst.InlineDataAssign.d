/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Declare different types of inline data types.
 *
 * SECTION: Type and Constant Definitions/Inlines
 *
 * NOTES: The commented lines defining floats and doubles should be uncommented
 * once the functionality is provided.
 *
 */

#pragma D option quiet


inline char new_char = 'c';
inline short new_short = 10;
inline int new_int = 100;
inline long new_long = 1234567890;
inline long long new_long_long = 1234512345;
inline int8_t new_int8 = 'p';
inline int16_t new_int16 = 20;
inline int32_t new_int32 = 200;
inline int64_t new_int64 = 2000000;
inline intptr_t new_intptr = 0x12345;
inline uint8_t new_uint8 = 'q';
inline uint16_t new_uint16 = 30;
inline uint32_t new_uint32 = 300;
inline uint64_t new_uint64 = 3000000;
inline uintptr_t new_uintptr = 0x67890;
/* inline float new_float = 1.23456;
inline double new_double = 2.34567890;
inline long double new_long_double = 3.567890123;
*/

inline unsigned long * pointer = &`max_pfn;

BEGIN
{
	exit(0);
}
