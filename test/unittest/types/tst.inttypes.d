/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify integer type aliases
 *
 * SECTION: Types, Operators, and Expressions/Data Types and Sizes
 */

BEGIN
{
	printf("sizeof(int8_t) = %u\n", sizeof(int8_t));
	printf("sizeof(int16_t) = %u\n", sizeof(int16_t));
	printf("sizeof(int32_t) = %u\n", sizeof(int32_t));
	printf("sizeof(int64_t) = %u\n", sizeof(int64_t));
	printf("sizeof(intptr_t) = %u\n", sizeof(intptr_t));
	printf("sizeof(uint8_t) = %u\n", sizeof(uint8_t));
	printf("sizeof(uint16_t) = %u\n", sizeof(uint16_t));
	printf("sizeof(uint32_t) = %u\n", sizeof(uint32_t));
	printf("sizeof(uint64_t) = %u\n", sizeof(uint64_t));
	printf("sizeof(uintptr_t) = %u\n", sizeof(uintptr_t));

	exit(0);
}

